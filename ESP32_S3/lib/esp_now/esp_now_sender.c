#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_timer.h" 

#include "g29_config.h"
#include "config.h"
#include "esp_now_sender.h"

static const char *TAG = "ESP_NOW_SENDER";
uint8_t target_car_mac[6];

// RTOS Nesneleri
static SemaphoreHandle_t telemetry_mutex = NULL;
static TaskHandle_t espnow_tx_task_handle = NULL;
static g29_telemetry_t shared_telemetry;

// Dışarıdan erişilen zaman değişkenleri
extern volatile int64_t last_g29_input_time;
extern const int64_t INACTIVITY_TIMEOUT_US;

// --- YENİ: Sistemin uyuyup uyumadığını takip eden bayrak ---
static volatile bool is_sleeping = true; 

// ESP-NOW Görevi
static void esp_now_sender_task(void *arg) {
    g29_telemetry_t packet;
    
    while (1) {
        int64_t current_time = esp_timer_get_time();

        // 1. DURUM: AKTİF MOD (30 Saniye dolmadıysa)
        if ((current_time - last_g29_input_time) < INACTIVITY_TIMEOUT_US) {
            
            is_sleeping = false; // Uyandık, aktif moddayız

            // Veriyi güvenle RAM'den al
            if (xSemaphoreTake(telemetry_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                packet = shared_telemetry;
                xSemaphoreGive(telemetry_mutex);
                
                // Havaya fırlat
                esp_now_send(target_car_mac, (uint8_t *)&packet, sizeof(g29_telemetry_t));
            }

            // --- SIKI 50HZ KİLİDİ ---
            // Görevi tam olarak 20ms uyutuyoruz. Bu sırada dışarıdan 1000 kere dürtülse bile 
            // uyanmaz, ritmini bozmaz. Tam 50Hz (saniyede 50 paket) gönderir.
            vTaskDelay(pdMS_TO_TICKS(20)); 
            
            // 20ms'lik bekleme süresince birikmiş olabilecek gereksiz uyandırma sinyallerini çöpe at.
            ulTaskNotifyTake(pdTRUE, 0); 
            
        } 
        // 2. DURUM: UYKU MODU (30 Saniye Hareketsizlik)
        else {
            if (!is_sleeping) {
                ESP_LOGW(TAG, "30 Saniye hareketsizlik. Uykuya geciliyor...");
                is_sleeping = true;
            }
            
            // SÜRESİZ UYKU: Sadece xTaskNotifyGive çağrıldığında (ilk harekette) uyanır
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            
            ESP_LOGI(TAG, "Hareket algilandi! 50Hz yayin tekrar basliyor.");
        }
    }
}

// ... (init_esp_now_sender fonksiyonun öncekiyle tamamen aynı kalıyor) ...
void init_esp_now_sender(void) {
    memcpy(target_car_mac, CAR_MAC_TABLE[ACTIVE_CAR_ID], 6);
    telemetry_mutex = xSemaphoreCreateMutex();
    // NVS ve Wi-Fi kurulumları... (Buraları aynen kopyalayabilirsin)
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


    ESP_LOGI(TAG, "Aktif Araç ID: %d | Hedef MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             ACTIVE_CAR_ID,
             target_car_mac[0], target_car_mac[1], target_car_mac[2],
             target_car_mac[3], target_car_mac[4], target_car_mac[5]);

    // ... Görev oluşturma ...
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



// Ana uygulamadan çağrılan veri gönderme fonksiyonu
void send_telemetry_to_car(const g29_telemetry_t *telemetry) {
    if (telemetry_mutex != NULL) {
        // 1. Veriyi her zaman güvenle belleğe (shared_telemetry) yaz
        if (xSemaphoreTake(telemetry_mutex, portMAX_DELAY) == pdTRUE) {
            shared_telemetry = *telemetry;
            xSemaphoreGive(telemetry_mutex);
        }
        
        // --- KRİTİK DEĞİŞİKLİK ---
        // Sadece sistem uyuyorsa görevi anında uyandırmak için dürt.
        // Zaten uyanıksa, bırak kendi 50Hz'lik ritminde (vTaskDelay içinde) takılsın.
        if (is_sleeping && espnow_tx_task_handle != NULL) {
            xTaskNotifyGive(espnow_tx_task_handle);
        }
    }
}