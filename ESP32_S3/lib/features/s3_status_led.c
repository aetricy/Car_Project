#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "s3_status_led.h"
#include "CONFIG.h" 

extern volatile s3_logic_state_t current_system_state;
static led_strip_handle_t led_strip;

// Kütüphaneye doğrudan sinyal yollayan fonksiyon (G ve B donanımsal ters çevrilmiş)
static void set_led_raw(uint32_t r, uint32_t g, uint32_t b) {
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

// LED'i tamamen kapatan fonksiyon
static void clear_led() {
    led_strip_set_pixel(led_strip, 0, 0, 0, 0);
    led_strip_refresh(led_strip);
}
static void s3_led_blink_task(void *pvParameters) {
    // İlk açılışta eski durumu "Bilinmeyen" (-1) yapıyoruz ki döngüye girince hemen rengi güncellesin
    s3_logic_state_t last_state = (s3_logic_state_t)-1; 

    while (1) {
        s3_logic_state_t current = current_system_state;

        // ==============================================================
        // SADECE DURUM DEĞİŞTİĞİNDE LED'İ GÜNCELLE (SIFIR CPU YÜKÜ)
        // ==============================================================
        if (current != last_state) {
            
            switch (current) {
                case STATE_USB_SETUP:
                case STATE_USB_WAITING:
                    set_led_raw(0, 0, 15);  // SABİT MAVİ
                    break;

                case STATE_USB_ENUMERATING:
                    set_led_raw(15, 15, 0); // SABİT SARI
                    break;
                    
                case STATE_SYS_ACTIVE:
                    set_led_raw(0, 15, 0);  // SABİT YEŞİL (Araba kullanımdayken RMT hiç çalışmaz)
                    break;

                case STATE_USB_DISCONNECTED:
                    set_led_raw(15, 0, 0);  // SABİT KIRMIZI
                    break;

                case STATE_SLEEP:
                    set_led_raw(10, 0, 15); // SABİT MOR
                    break;

                case STATE_DEV_MODE:
                    set_led_raw(0, 15, 15); // SABİT TURKUAZ / CYAN (Dev Mod)
                    break;
                    
                default:
                    set_led_raw(0, 0, 0);   // KAPALI
                    break;
            }
            
            // Yeni durumu kaydet ki bir daha aynı rengi tekrar tekrar yollamasın
            last_state = current;
        }

        // Görev sadece 200ms'de bir uyanıp duruma bakar. 
        // Eğer durum aynıysa hiçbir şey yapmadan tekrar uyur.
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


void init_s3_status_led(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = S3_RGB_LED_PIN,
        .max_leds = 1, 
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, 
    };
    
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    clear_led();

    xTaskCreatePinnedToCore(
        s3_led_blink_task, 
        "s3_rgb_task", 
        2048, 
        NULL, 
        1, 
        NULL, 
        OTHER_TASK_CORE
    );
}