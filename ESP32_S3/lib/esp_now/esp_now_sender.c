#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "g29_config.h"
#include "config.h"
#include "esp_now_sender.h"

static const char *TAG = "ESP_NOW_SENDER";
uint8_t target_car_mac[6];

// RTOS Nesneleri (Artık hafif car_drive_packet_t paylaşıyor)
static SemaphoreHandle_t drive_mutex = NULL;
static TaskHandle_t espnow_tx_task_handle = NULL;
static car_drive_packet_t shared_drive_packet;


// ESP-NOW Gönderici Task
static void esp_now_sender_task(void *arg) {
    car_drive_packet_t packet;
    
    while (1) {
        // 1. BEKLEME (UYKU) NOKTASI
        // main.c'den xTaskNotifyGive() gelene kadar burada SÜRESİZ bekler.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 2. UYANDIK! Güncel hafif paketi Mutex ile güvenle al
        if (xSemaphoreTake(drive_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            packet = shared_drive_packet;
            xSemaphoreGive(drive_mutex);
            
            // 3. Havaya fırlat (Sadece 5 baytlık optimize paket)
            esp_now_send(target_car_mac, (uint8_t *)&packet, sizeof(car_drive_packet_t));
        }

        // 4. --- SIKI 50HZ KİLİDİ ---
        vTaskDelay(pdMS_TO_TICKS(40)); 
        
        // Birikmiş uyanma sinyallerini temizle
        ulTaskNotifyTake(pdTRUE, 0); 
    }
}



void init_esp_now_sender(void) {
    memcpy(target_car_mac, CAR_MAC_TABLE[ACTIVE_CAR_ID], 6);
    drive_mutex = xSemaphoreCreateMutex();
    
    // NVS Kurulumu
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. Wi-Fi ve ESP-NOW Başlatma
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());

    // 2. Peer (Araç) Ekleme
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, target_car_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    ESP_LOGI(TAG, "Aktif Araç ID: %d | Hedef MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             ACTIVE_CAR_ID,
             target_car_mac[0], target_car_mac[1], target_car_mac[2],
             target_car_mac[3], target_car_mac[4], target_car_mac[5]);

    // Görev oluşturma
    xTaskCreatePinnedToCore(
        esp_now_sender_task,   
        "esp_now_sender",      
        4096,                  
        NULL,                  
        4,                     
        &espnow_tx_task_handle,
        OTHER_TASK_CORE        
    );
}




// Ana uygulamadan (Artık ESPNOW_Task içindeki kuyruktan) çağrılan fonksiyon
void send_telemetry_to_car(const car_drive_packet_t *packet) {
    if (drive_mutex != NULL) {
        
        // 1. Hafif sürüş paketini güvenle shared belleğe yaz
        if (xSemaphoreTake(drive_mutex, portMAX_DELAY) == pdTRUE) {
            shared_drive_packet = *packet;
            xSemaphoreGive(drive_mutex);
        }
        
        // 2. Sender Task'ı uyandır
        if (espnow_tx_task_handle != NULL) {
            xTaskNotifyGive(espnow_tx_task_handle);
        }
    }
}