#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_indicator.h"


#define STATE_WAITING  1
#define STATE_ACTIVE   2
#define STATE_FAILSAFE 3
#define STATE_SLEEP    4


// main.c dosyasındaki global değişkeni referans alıyoruz
extern volatile int current_state;

static void led_blink_task(void *pvParameters) {
    // GPIO Kurulumu
    gpio_reset_pin(BUILTIN_LED_PIN);
    gpio_set_direction(BUILTIN_LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        switch (current_state) {
            
            case STATE_WAITING:
                // Yavaş Yan/Sön (1 saniye döngü)
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case STATE_ACTIVE:
                // Kalp Atışı: Çift kısa pırpır, ardından uzun bekleme
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                
                vTaskDelay(pdMS_TO_TICKS(1700)); // 1.7 saniye bekle
                break;

            case STATE_FAILSAFE:
                // Hızlı Flaşör (Tehlike Alarmı)
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case STATE_SLEEP:
                // Uyku: Çoğunlukla kapalı, 3 saniyede bir çok kısa bir pırıltı
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(2950));
                break;
                
            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

void init_led_indicator(void) {
    // LED Task'ını arka planda çalışmak üzere başlatıyoruz (Öncelik: 1 - Düşük)
    xTaskCreate(led_blink_task, "led_task", 2048, NULL, 1, NULL);
}