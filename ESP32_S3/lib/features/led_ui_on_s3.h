#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h" // ESP-IDF Adreslenebilir LED kütüphanesi
#include "config.h"

// --------------------------------------------------
// DONANIM AYARLARI (Kendi pinine göre değiştir)
// (ESP32-S3 DevKit'lerin üzerindeki dahili RGB LED genelde 48. pindedir)
#define WS2812_PIN  48  
#define NUM_LEDS    1   
// --------------------------------------------------

extern volatile s3_logic_state_t current_system_state;

void LED_UI_Task(void *pvParameters) {
    
    // 1. WS2812B Kurulum Ayarları (RMT Donanımını Kullanır)
    led_strip_handle_t led_strip;
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_PIN,
        .max_leds = NUM_LEDS,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz çözünürlük
    };
    
    if (led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip) != ESP_OK) {
        ESP_LOGE("LED_UI", "Kritik Hata: LED Strip başlatılamadı!");
        vTaskDelete(NULL); // Hata varsa taskı yok et, sistemi çökertme
    }

    led_strip_clear(led_strip);
    bool toggle = false;

    ESP_LOGI("LED_UI", "WS2812B Arayüzü Başladı.");

    while(1) {
        switch (current_system_state) {
            
            case STATE_SYS_ACTIVE:
                // SÜRÜŞ AKTİF: Sabit Yeşil (R:0, G:255, B:0)
                led_strip_set_pixel(led_strip, 0, 0, 255, 0); 
                led_strip_refresh(led_strip);
                vTaskDelay(pdMS_TO_TICKS(100)); 
                break;

            case STATE_USB_WAITING:
            case STATE_USB_SETUP:
                // BEKLEME: Mavi Yanıp Sönme (R:0, G:0, B:255)
                if(toggle) led_strip_set_pixel(led_strip, 0, 0, 0, 255);
                else led_strip_clear(led_strip);
                
                led_strip_refresh(led_strip);
                toggle = !toggle;
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case STATE_USB_ENUMERATING:
                // TANIMLANMA: Mor Hızlı Flaşör (R:128, G:0, B:128)
                if(toggle) led_strip_set_pixel(led_strip, 0, 128, 0, 128);
                else led_strip_clear(led_strip);
                
                led_strip_refresh(led_strip);
                toggle = !toggle;
                vTaskDelay(pdMS_TO_TICKS(50));
                break;

            case STATE_USB_DISCONNECTED:
                // KOPTU / HATA: Kırmızı Hızlı Flaşör (R:255, G:0, B:0)
                if(toggle) led_strip_set_pixel(led_strip, 0, 255, 0, 0);
                else led_strip_clear(led_strip);
                
                led_strip_refresh(led_strip);
                toggle = !toggle;
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case STATE_SLEEP:
                // UYKU MODU: Zayıf Sarı Nefes Alma (R:20, G:15, B:0)
                // Gece göz almaması için ışık gücü çok düşük tutuldu
                led_strip_set_pixel(led_strip, 0, 20, 15, 0); 
                led_strip_refresh(led_strip);
                vTaskDelay(pdMS_TO_TICKS(50)); // Kısacık yanar
                
                led_strip_clear(led_strip);
                vTaskDelay(pdMS_TO_TICKS(1500)); // Uzunca bekler
                break;
                
            default:
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}