#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "led_indicator.h"

#define STATE_WAITING  1
#define STATE_ACTIVE   2
#define STATE_FAILSAFE 3
#define STATE_SLEEP    4

// Reference global system state from main.c
extern volatile int current_state;

static void led_blink_task(void *pvParameters) {
    // GPIO Configuration
    gpio_reset_pin(BUILTIN_LED_PIN);
    gpio_set_direction(BUILTIN_LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        switch (current_state) {
            
            case STATE_WAITING:
                // Slow blink (1-second cycle: 500ms ON, 500ms OFF)
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case STATE_ACTIVE:
                // Heartbeat: Double short pulse followed by pause
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                
                vTaskDelay(pdMS_TO_TICKS(1700)); // Wait 1.7s
                break;

            case STATE_FAILSAFE:
                // Rapid strobe (Failsafe alarm)
                gpio_set_level(BUILTIN_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(BUILTIN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case STATE_SLEEP:
                // Sleep: Mostly off, brief 50ms blip every 3 seconds
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
    // Start LED indicator task in background (Priority: 1 - Low)
    xTaskCreate(led_blink_task, "led_task", 2048, NULL, 1, NULL);
}