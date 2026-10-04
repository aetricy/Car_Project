#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "g29_config.h"
#include "esp_now_receive.h"
#include "pwm_control.h" 

// --- LOGLAMA MAKROSU ---
#define DEBUG_LOG_ENABLE  1  
#if DEBUG_LOG_ENABLE
    #define RC_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define RC_PRINT(fmt, ...) 
#endif

// main.c dosyasındaki global değişkenlere erişim
extern volatile uint16_t raw_steering_us;
extern volatile uint16_t raw_throttle_us;
extern volatile int current_state; 

#define STATE_WAITING  1
#define STATE_ACTIVE   2
#define STATE_FAILSAFE 3
#define STATE_SLEEP    4

// FreeRTOS Timer yerine, paketin geldiği son anı tutacak değişken
volatile uint32_t last_packet_time = 0; 

// Güvenli veri okuma için dahili değişkenler
static car_drive_packet_t latest_drive_packet;
static volatile bool new_data_available = false;

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(car_drive_packet_t)) {
        // 1. Veriyi HIZLICA kopyala
        memcpy(&latest_drive_packet, data, sizeof(car_drive_packet_t));
        
        // 2. Yeni veri var bayrağını kaldır
        new_data_available = true;
    }
}

void init_esp_now_receiver(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    RC_PRINT("\n==================================================\n");
    RC_PRINT("ESP32-C3 Alici MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    RC_PRINT("==================================================\n\n");

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    RC_PRINT("[INFO] ESP-NOW Alici modulu basariyla baslatildi.\n");
}

// Ana döngünün veriyi güvenle çekmesi için yazılmış API Fonksiyonu
bool esp_now_get_latest_data(car_drive_packet_t *out_data) {
    if (new_data_available) {
        memcpy(out_data, &latest_drive_packet, sizeof(car_drive_packet_t));
        new_data_available = false;
        return true;
    }
    return false;
}