#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "s3_status_led.h"
#include "CONFIG.h" 

extern volatile s3_logic_state_t current_system_state;
static led_strip_handle_t led_strip;

// Direct LED drive function
static void set_led_raw(uint32_t r, uint32_t g, uint32_t b) {
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

// Turn off LED completely
static void clear_led() {
    led_strip_set_pixel(led_strip, 0, 0, 0, 0);
    led_strip_refresh(led_strip);
}
static void s3_led_blink_task(void *pvParameters) {
    // Initialize last state to -1 to trigger immediate color update upon entry
    s3_logic_state_t last_state = (s3_logic_state_t)-1; 

    while (1) {
        s3_logic_state_t current = current_system_state;

        // ==============================================================
        // ONLY UPDATE LED WHEN SYSTEM STATE CHANGES (ZERO CPU OVERHEAD)
        // ==============================================================
        if (current != last_state) {
            
            switch (current) {
                case STATE_USB_SETUP:
                case STATE_USB_WAITING:
                    set_led_raw(0, 0, 15);  // SOLID BLUE
                    break;

                case STATE_USB_ENUMERATING:
                    set_led_raw(15, 15, 0); // SOLID YELLOW
                    break;
                    
                case STATE_SYS_ACTIVE:
                    set_led_raw(0, 15, 0);  // SOLID GREEN (During active drive, RMT remains idle)
                    break;

                case STATE_USB_DISCONNECTED:
                    set_led_raw(15, 0, 0);  // SOLID RED
                    break;

                case STATE_SLEEP:
                    set_led_raw(10, 0, 15); // SOLID PURPLE
                    break;

                case STATE_DEV_MODE:
                    set_led_raw(0, 15, 15); // SOLID CYAN (Dev Mode)
                    break;
                    
                default:
                    set_led_raw(0, 0, 0);   // OFF
                    break;
            }
            
            // Save new state so identical color is not re-transmitted
            last_state = current;
        }

        // Task polls state every 200ms. If state is unchanged, it returns to sleep.
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