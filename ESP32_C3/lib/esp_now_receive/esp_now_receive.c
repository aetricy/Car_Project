#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"

#include "esp_now_receive.h"

static const char *TAG = "ESP_NOW_RX";

// Sadece bu dosyaya özel (static) değişkenler. Dışarıdan doğrudan erişilemez!
static g29_telemetry_t latest_telemetry;
static volatile bool new_data_available = false;

// ESP-NOW Paket Alım (Receive) Callback - IŞIK HIZINDA ÇALIŞMALI
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(g29_telemetry_t)) {
        // 1. Veriyi HIZLICA kopyala
        memcpy(&latest_telemetry, data, sizeof(g29_telemetry_t));
        
        // 2. Yeni veri var bayrağını kaldır
        new_data_available = true;
    }
}

void init_esp_now_receiver(void) {
    // 1. NVS Başlat
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Wi-Fi ve İstasyon Modunu Başlat
    ESP_ERROR_CHECK(esp_netif_init());
    
    // (Olası Core paniklerini engellemek için varsayılan olay döngüsünü ekledik)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 3. Kartın Kendi MAC Adresini Oku ve Yazdır
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    printf("\n==================================================\n");
    printf("ESP32-C3 Alici MAC Adresi: %02X:%02X:%02X:%02X:%02X:%02X\n", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("==================================================\n\n");

    // 4. ESP-NOW Başlat ve Callback Kaydet
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    ESP_LOGI(TAG, "ESP-NOW Alici modulu basariyla baslatildi.");
}

// Ana döngünün veriyi güvenle çekmesi için yazılmış API Fonksiyonu
bool esp_now_get_latest_data(g29_telemetry_t *out_data) {
    if (new_data_available) {
        // Veriyi dışarıya (main'e) kopyala
        memcpy(out_data, &latest_telemetry, sizeof(g29_telemetry_t));
        // Bayrağı indir ki aynı paketi iki kere okumayalım
        new_data_available = false;
        return true;
    }
    return false;
}