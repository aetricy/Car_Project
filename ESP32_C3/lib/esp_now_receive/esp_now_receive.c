#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "g29_config.h"
#include "esp_now_receive.h"
#include "pwm_control.h" 

#define DEBUG_LOG_ENABLE  1  
#if DEBUG_LOG_ENABLE
    #define RC_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define RC_PRINT(fmt, ...) 
#endif

extern volatile uint16_t raw_steering_us;
extern volatile uint16_t raw_throttle_us;
extern volatile int current_state; 

#define STATE_WAITING  1
#define STATE_ACTIVE   2
#define STATE_FAILSAFE 3
#define STATE_SLEEP    4

volatile uint32_t last_packet_time = 0; 

// --- DATA STORAGE BUFFERS ---
// 1. Drive telemetry remains global (continually updated, safe to overwrite)
static car_drive_packet_t latest_drive_packet;
static volatile bool new_data_available = false;

// 2. Queues for commands and config packets (must not be dropped)
QueueHandle_t rx_command_queue;
QueueHandle_t rx_config_queue;

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == 0 || data == NULL) return;

    // Read incoming packet type from first byte
    uint8_t packet_type = data[0];

    if (packet_type == PKT_TYPE_DRIVE && len == sizeof(car_drive_packet_t)) {
        memcpy(&latest_drive_packet, data, len);
        new_data_available = true;
    } 
    else if (packet_type == PKT_TYPE_COMMAND && len == sizeof(car_command_packet_t)) {
        // Enqueue inside callback (non-blocking)
        xQueueSend(rx_command_queue, data, 0);
    } 
    else if (packet_type == PKT_TYPE_CONFIG && len == sizeof(car_config_packet_t)) {
        xQueueSend(rx_config_queue, data, 0);
    }
}

void init_esp_now_receiver(void) {
    // Create receiver queues
    rx_command_queue = xQueueCreate(5, sizeof(car_command_packet_t));
    rx_config_queue  = xQueueCreate(3, sizeof(car_config_packet_t));

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
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));                   // Disable Wi-Fi power save for lowest latency
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));   // Fixed Wi-Fi Channel 1

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    RC_PRINT("\n==================================================\n");
    RC_PRINT("ESP32-C3 Receiver MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    RC_PRINT("==================================================\n\n");

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    RC_PRINT("[INFO] ESP-NOW receiver module initialized successfully.\n");
}

// Fetch drive data (polled by main task)
bool esp_now_get_latest_data(car_drive_packet_t *out_data) {
    if (new_data_available) {
        memcpy(out_data, &latest_drive_packet, sizeof(car_drive_packet_t));
        new_data_available = false;
        return true;
    }
    return false;
}

// Fetch command data (polled by main task)
bool esp_now_get_command_data(car_command_packet_t *out_data) {
    if (rx_command_queue != NULL) {
        return (xQueueReceive(rx_command_queue, out_data, 0) == pdTRUE);
    }
    return false;
}

// Fetch config data (polled by main task)
bool esp_now_get_config_data(car_config_packet_t *out_data) {
    if (rx_config_queue != NULL) {
        return (xQueueReceive(rx_config_queue, out_data, 0) == pdTRUE);
    }
    return false;
}