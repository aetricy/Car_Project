#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "g29_config.h"

g29_telemetry_t latest_telemetry;
int64_t last_packet_time = 0;
const int64_t FAILSAFE_TIMEOUT_US = 500000; // 500 ms (Mikro saniye cinsinden)

// ESP-NOW Paket Alım (Receive) Callback
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    if (len == sizeof(g29_telemetry_t)) {
        memcpy(&latest_telemetry, incomingData, sizeof(g29_telemetry_t));
        last_packet_time = esp_timer_get_time(); // Anlık mikro saniye zamanını al

        // Standart printf ile terminale bas
        printf("Paket Alindi -> Steering: %5.2f | Throttle: %4.2f | Brake: %4.2f\n", 
                 latest_telemetry.steering, 
                 latest_telemetry.throttle, 
                 latest_telemetry.brake);
    }
}

// ESP-NOW Başlatma
void init_esp_now_receiver() {
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));
}

void app_main(void) {
    // 1. NVS Başlat (Wi-Fi için şart)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. ESP-NOW Alıcısını Kur
    init_esp_now_receiver();
    
    printf("ESP32-C3 Dinlemede (printf Modu)... S3 Verisi Bekleniyor.\n");

    // 3. Ana Döngü (Bağlantı Kopma Kontrolü)
    while (1) {
        int64_t current_time = esp_timer_get_time();
        
        // Eğer 500ms boyunca S3'ten paket gelmezse bağlantı koptu uyarısı ver
        if ((current_time - last_packet_time > FAILSAFE_TIMEOUT_US) && (last_packet_time != 0)) {
            printf("UYARI: Baglanti koptu: S3'ten sinyal alinamiyor!\n");
            last_packet_time = current_time; 
        }

        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}