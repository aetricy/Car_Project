#include "led_ui_on_g29.h"
#include "g29_driver_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void g29_led_ui_set_raw(uint8_t mask) {
    g29_set_leds(mask & G29_LED_ALL);
}

void g29_led_ui_clear(void) {
    g29_set_leds(G29_LED_NONE);
}

void g29_led_ui_show_bar(uint8_t level) {
    switch (level) {
        case 0: g29_led_ui_set_raw(0x00); break;
        case 1: g29_led_ui_set_raw(0x01); break; // 1 LED (Green 1)
        case 2: g29_led_ui_set_raw(0x03); break; // 2 LEDs (Green 1+2)
        case 3: g29_led_ui_set_raw(0x07); break; // 3 LEDs (Green 1+2 + Yellow 1)
        case 4: g29_led_ui_set_raw(0x0F); break; // 4 LEDs (Green 1+2 + Yellow 1+2)
        case 5:
        default:
            g29_led_ui_set_raw(0x1F); break;     // 5 LEDs (All on)
    }
}

void g29_led_ui_show_menu_single(uint8_t menu_index, bool is_on) {
    if (!is_on) {
        g29_led_ui_clear();
        return;
    }
    if (menu_index == 5) {
        // Menu 6 (Gyro Gain): Green 1 and Red LED light up together
        g29_led_ui_set_raw(G29_LED_1_GREEN1 | G29_LED_5_RED);
        return;
    }
    if (menu_index == 6) {
        // Menu 7 (Vehicle Select): Green 2 + Yellow 1 + Yellow 2 (3 middle LEDs) light up together
        g29_led_ui_set_raw(G29_LED_2_GREEN2 | G29_LED_3_YELLOW1 | G29_LED_4_YELLOW2);
        return;
    }
    if (menu_index > 6) {
        g29_led_ui_clear();
        return;
    }
    uint8_t mask = (1 << menu_index);
    g29_led_ui_set_raw(mask);
}

void g29_led_ui_show_trim_position(int8_t trim_pos) {
    if (trim_pos <= -2) {
        g29_led_ui_set_raw(G29_LED_1_GREEN1); // Full Left Trim
    } else if (trim_pos == -1) {
        g29_led_ui_set_raw(G29_LED_2_GREEN2); // Slight Left Trim
    } else if (trim_pos == 0) {
        g29_led_ui_set_raw(G29_LED_3_YELLOW1); // Dead Center (Yellow 1)
    } else if (trim_pos == 1) {
        g29_led_ui_set_raw(G29_LED_4_YELLOW2); // Slight Right Trim
    } else {
        g29_led_ui_set_raw(G29_LED_5_RED);     // Full Right Trim
    }
}

void g29_led_ui_intro_animation(void) {
    // 3 quick flashes
    for (int i = 0; i < 3; i++) {
        g29_led_ui_set_raw(G29_LED_ALL);
        vTaskDelay(pdMS_TO_TICKS(70));
        g29_led_ui_clear();
        vTaskDelay(pdMS_TO_TICKS(70));
    }
}

void g29_led_ui_exit_animation(void) {
    // Sweep-off animation
    for (int i = 4; i >= 0; i--) {
        uint8_t mask = (1 << (i + 1)) - 1;
        g29_led_ui_set_raw(mask);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    g29_led_ui_clear();
}

static uint8_t s_last_throttle_mask = 0xFF;
static uint32_t s_shift_blink_tick = 0;

void g29_led_ui_reset_throttle_cache(void) {
    s_last_throttle_mask = 0xFF;
    s_shift_blink_tick = 0;
}

void g29_led_ui_update_throttle(float throttle) {
    uint8_t target_mask = 0;

    // Progressive LED illumination based on throttle pedal
    if (throttle < 0.10f) {
        target_mask = G29_LED_NONE; // 0% - 10%: Idle, all LEDs off
    } else if (throttle < 0.30f) {
        target_mask = 0x01; // 10% - 30%: 1 LED (Green 1)
    } else if (throttle < 0.50f) {
        target_mask = 0x03; // 30% - 50%: 2 LEDs (Green 1+2)
    } else if (throttle < 0.70f) {
        target_mask = 0x07; // 50% - 70%: 3 LEDs (Green 1+2 + Yellow 1)
    } else if (throttle < 0.90f) {
        target_mask = 0x0F; // 70% - 90%: 4 LEDs (Green 1+2 + Yellow 1+2)
    } else if (throttle < 0.96f) {
        target_mask = 0x1F; // 90% - 96%: 5 LEDs (Green + Yellow + Red solid on)
    } else {
        // 96%+ (Full throttle / Rev Limiter / Shift Light flash effect)
        // Flash all LEDs every ~80ms
        s_shift_blink_tick++;
        if ((s_shift_blink_tick / 4) % 2 == 0) {
            target_mask = 0x1F;
        } else {
            target_mask = 0x00;
        }
    }

    // Only transmit over USB when LED mask changes
    if (target_mask != s_last_throttle_mask) {
        g29_led_ui_set_raw(target_mask);
        s_last_throttle_mask = target_mask;
    }
}

