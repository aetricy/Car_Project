#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h" // Zaman ölçümü için gerekli


#include "CONFIG.h"

// Son direksiyon hareketinin zamanını tutacağımız değişken
volatile int64_t last_g29_input_time = 0; 
const int64_t INACTIVITY_TIMEOUT_US = INACTIVITY_TIMEOUT_us; // 30 Saniye (Mikrosaniye cinsinden)


#include "g29_driver_host.h" // Modüler kütüphanemiz
#include "g29_processor.h"

#include "esp_now_sender.h"

static const char *TAG = "MAIN_APP";



g29_telemetry_t current_telemetry;
// -------------------------------------------------------------
// 1. G29 GİRDİ (INPUT) CALLBACK
// -------------------------------------------------------------
void on_g29_input_received(const uint8_t *data, int len) {
    if(g29_is_ready()){
        
        last_g29_input_time = esp_timer_get_time();
        
        // Ham veriyi telemetri struct'ına dönüştür
        g29_process_raw_data(data, len, &current_telemetry);
         
        // ESP-NOW kütüphanemize yolla (O da Mutex'e yazıp görevi uyandıracak)
        send_telemetry_to_car(&current_telemetry);



        if (LOG_WHEELSTATE){
            if (len >= 8) {
                // --- HAM VERİYİ (RAW DATA) LOGLAMA EKLENDİ ---
                // G29 raporları genellikle en az 8 bayttır. İlk 8 baytı ekrana basıyoruz.
                ESP_LOGI("RAW_DATA", "[Len:%d] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", 
                        len, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[11]);
            }
            // İşlenmiş veriyi logla (Çok hızlı akarsa yorum satırına alabilirsin)
            ESP_LOGI("TELEMETRY", "Steering: %5.2f | Throttle: %4.2f | Brake: %4.2f | Clutch: %4.2f | BTN : +:%d / -:%d",  
                    current_telemetry.steering, 
                    current_telemetry.throttle, 
                    current_telemetry.brake,
                    current_telemetry.clutch,
                    current_telemetry.button_plus,
                    current_telemetry.button_minus
                );
            }
    }
}

// -------------------------------------------------------------
// 2. G29 DURUM (STATE) CALLBACK ve INIT Settings
// -------------------------------------------------------------
void on_g29_state_changed(g29_state_t state) {
    switch (state) {
        case G29_STATE_DISCONNECTED:
            ESP_LOGW(TAG, "G29 Bağlantısı Koptu.");
            break;
            
        case G29_STATE_PS3_WAKING_UP:
            ESP_LOGI(TAG, "G29 PS3 Modunda tespit edildi....");
            break;
            
        case G29_STATE_NATIVE_READY:

            ESP_LOGI(TAG, "==================================================");
            ESP_LOGI(TAG, " G29 HAZIR!");
            ESP_LOGI(TAG, "==================================================");


            // INIT SETTINGS IN HERE...
            init_esp_now_sender();

            // G29 Config Init
            g29_disable_autocenter();
            g29_set_range(540);


            break;
    }
}

// -------------------------------------------------------------
// 4. ANA FONKSİYON
// -------------------------------------------------------------
void app_main(void) {
    ESP_LOGI(TAG, "Sistem Başlatılıyor (Core 0)...");

    
    if (g29_init(on_g29_state_changed, on_g29_input_received) == ESP_OK) {
        ESP_LOGI(TAG, "Sürücüsü Başarıyla Kuruldu. USB Bekleniyor...");

        // OTHER TASKS IN HERE...


        
    } else {
        ESP_LOGE(TAG, "Sürücüsü Başlatılamadı!");
    }
    
}


    