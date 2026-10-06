#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "g29_config.h"
#include "CONFIG.h"
#include "esp_now_sender.h"

static const char *TAG = "ESP_NOW_SENDER";

uint8_t target_car_mac[6];
static uint8_t s_active_car_id = ACTIVE_CAR_ID;
static bool s_esp_now_is_initialized = false;

static volatile bool s_car_connected = false;
static volatile bool s_car_reconnected = false;
static volatile uint32_t s_consecutive_tx_fails = 0;

extern QueueHandle_t espnow_tx_queue;
static TaskHandle_t espnow_tx_task_handle = NULL;

// ESP-NOW Gönderim Durum Callback (Geri Bildirim)
static void on_esp_now_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        s_consecutive_tx_fails = 0;
        if (!s_car_connected) {
            s_car_connected = true;
            s_car_reconnected = true; // Araç yeni açıldı veya menzile girdi!
            ESP_LOGI(TAG, ">>> ARAÇ RF BAĞLANTISI AKTİF (Online)! <<<");
        }
    } else {
        s_consecutive_tx_fails++;
        if (s_consecutive_tx_fails >= 25 && s_car_connected) { // ~500ms yanıtsızlık
            s_car_connected = false;
            ESP_LOGW(TAG, ">>> ARAÇ KOPUK / ÇEVRİMDIŞI (Offline) <<<");
        }
    }
}

// ESP-NOW Gönderici Task (Doğrudan Kuyruktan Okur)
static void esp_now_sender_task(void *arg) {
    espnow_tx_item_t item;
    
    while (1) {
        // Kuyrukta veri olana kadar SÜRESİZ bekler, CPU harcamaz
        if (xQueueReceive(espnow_tx_queue, &item, portMAX_DELAY) == pdTRUE) {
            // Sadece paketin gerçek boyutu kadarını havaya fırlat (Çöpler gitmez)
            esp_now_send(target_car_mac, (uint8_t *)&item.payload, item.length);
        }
    }
}

void init_esp_now_sender(void) {
    memcpy(target_car_mac, CAR_MAC_TABLE[s_active_car_id], 6);
    
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
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));                   // Kesintisiz ESP-NOW için güç tasarrufunu kapat
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));   // Sabit Kanal 1

    ESP_ERROR_CHECK(esp_now_init());
    s_esp_now_is_initialized = true;

    ESP_ERROR_CHECK(esp_now_register_send_cb(on_esp_now_send_cb));

    // 2. Peer (Araç) Ekleme
    bool is_zero = true;
    for (int i = 0; i < 6; i++) {
        if (target_car_mac[i] != 0) { is_zero = false; break; }
    }

    if (!is_zero) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, target_car_mac, 6);
        peerInfo.channel = 1;
        peerInfo.encrypt = false;
        ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
        ESP_LOGI(TAG, "Aktif Araç ID: %d | Hedef MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 s_active_car_id + 1,
                 target_car_mac[0], target_car_mac[1], target_car_mac[2],
                 target_car_mac[3], target_car_mac[4], target_car_mac[5]);
    } else {
        ESP_LOGW(TAG, "Aktif Araç ID: %d MAC adresi bos (00:00:00:00:00:00)!", s_active_car_id + 1);
    }

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

bool esp_now_sender_set_active_car(uint8_t car_id) {
    if (car_id >= CAR_MAX_COUNT) return false;

    s_active_car_id = car_id;
    memcpy(target_car_mac, CAR_MAC_TABLE[car_id], 6);

    // Eğer ESP-NOW henüz başlatılmadıysa (örneğin boot/NVS okuma aşamasında)
    // Sadece hedef MAC ve ID'yi ayarla, peer işlemlerini init_esp_now_sender()'a bırak
    if (!s_esp_now_is_initialized) {
        return true;
    }

    // Eski peer kayıtlıysa sil
    if (esp_now_is_peer_exist(target_car_mac)) {
        esp_now_del_peer(target_car_mac);
    }

    bool is_zero = true;
    for (int i = 0; i < 6; i++) {
        if (target_car_mac[i] != 0) { is_zero = false; break; }
    }

    if (!is_zero) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, target_car_mac, 6);
        peerInfo.channel = 1;
        peerInfo.encrypt = false;
        esp_err_t err = esp_now_add_peer(&peerInfo);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Yeni Hedef Araç Belirlendi -> [Araç %d] MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                     car_id + 1,
                     target_car_mac[0], target_car_mac[1], target_car_mac[2],
                     target_car_mac[3], target_car_mac[4], target_car_mac[5]);
        }
    } else {
        ESP_LOGW(TAG, "Secilen [Araç %d] MAC adresi tanimsiz (00:00:00:00:00:00)!", car_id + 1);
    }

    s_car_connected = false;
    s_car_reconnected = true; // Yeni araca bağlanınca anında config ve wake-up gönderilsin
    return true;
}

uint8_t esp_now_sender_get_active_car(void) {
    return s_active_car_id;
}

bool esp_now_sender_is_car_connected(void) {
    return s_car_connected;
}

bool esp_now_sender_check_and_clear_reconnected(void) {
    if (s_car_reconnected) {
        s_car_reconnected = false;
        return true;
    }
    return false;
}

