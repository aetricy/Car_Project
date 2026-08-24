#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"


#include "g29_config.h"
#include "config.h"

static const char *TAG = "ESP_NOW_SENDER";
uint8_t target_car_mac[6];

// ESP-NOW Kuyruk Yapısı (En son 5 paketi tutar, eskileri atar ki şişme yapmasın)
static QueueHandle_t telemetry_queue = NULL;

// ESP-NOW Core 0 Görevi
static void esp_now_sender_task(void *arg) {
    g29_telemetry_t packet;
    
    while (1) {
        // Kuyruktan yeni paket gelmesini bekle (Bloklanır, CPU'yu yormaz)
        if (xQueueReceive(telemetry_queue, &packet, portMAX_DELAY) == pdTRUE) {
            esp_err_t result = esp_now_send(target_car_mac, (uint8_t *)&packet, sizeof(g29_telemetry_t));
            
            // Hata takibi
             if (result != ESP_OK) {
                ESP_LOGW(TAG, "Gönderim hatası: %d", result);
            }
        }
    }
}

void init_esp_now_sender(void) {
    memcpy(target_car_mac, CAR_MAC_TABLE[ACTIVE_CAR_ID], 6);
    
    

    // --- EKLENEN KISIM: NVS FLASH BAŞLATMA ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // -----------------------------------------

    // 1. Wi-Fi ve ESP-NOW Başlatma (Hata veren kısım burasıydı)
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());

    // 2. Peer Ekleme
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, target_car_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    // 3. Kuyruğu Oluştur (5 eleman kapasiteli)
    telemetry_queue = xQueueCreate(5, sizeof(g29_telemetry_t));

    ESP_LOGI(TAG, "Aktif Araç ID: %d | Hedef MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             ACTIVE_CAR_ID,
             target_car_mac[0], target_car_mac[1], target_car_mac[2],
             target_car_mac[3], target_car_mac[4], target_car_mac[5]);

    // 4. GÖREVİ KESİN OLARAK CORE 0'A BAĞLA (Pinned to Core 0)
    xTaskCreatePinnedToCore(
        esp_now_sender_task,   // Çalışacak fonksiyon
        "esp_now_sender",      // Görev adı
        4096,                  // Stack boyutu
        NULL,                  // Parametre
        4,                     // Öncelik (Priority)
        NULL,                  // Task Handle
        OTHER_TASK_CORE        // <-- CORE 0 SEÇİLDİ
    );

    ESP_LOGI(TAG, "ESP-NOW Çalıştırıldı.");
}

// Artık bu fonksiyon veriyi direkt göndermek yerine kuyruğa atacak (Core-safe)
void send_telemetry_to_car(const g29_telemetry_t *telemetry) {
    if (telemetry_queue != NULL) {
        // xQueueSendToBack istersen anlık atar, 0 ise bekletmez
        xQueueSend(telemetry_queue, telemetry, 0);
    }
}