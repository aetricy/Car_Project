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
        case 1: g29_led_ui_set_raw(0x01); break; // 1 LED (Yeşil 1)
        case 2: g29_led_ui_set_raw(0x03); break; // 2 LED (Yeşil 1+2)
        case 3: g29_led_ui_set_raw(0x07); break; // 3 LED (Yeşil 1+2 + Sarı 1)
        case 4: g29_led_ui_set_raw(0x0F); break; // 4 LED (Yeşil 1+2 + Sarı 1+2)
        case 5:
        default:
            g29_led_ui_set_raw(0x1F); break;     // 5 LED (Tümü açık)
    }
}

void g29_led_ui_show_menu_single(uint8_t menu_index, bool is_on) {
    if (!is_on) {
        g29_led_ui_clear();
        return;
    }
    if (menu_index == 5) {
        // Menü 6 (Gyro Gain): Yeşil 1 ve Kırmızı LED birlikte yanar
        g29_led_ui_set_raw(G29_LED_1_GREEN1 | G29_LED_5_RED);
        return;
    }
    if (menu_index == 6) {
        // Menü 7 (Araç Seçimi): Yeşil 2 + Sarı 1 + Sarı 2 (3 orta LED) birlikte yanar
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
        g29_led_ui_set_raw(G29_LED_1_GREEN1); // Tam Sol Trim
    } else if (trim_pos == -1) {
        g29_led_ui_set_raw(G29_LED_2_GREEN2); // Hafif Sol Trim
    } else if (trim_pos == 0) {
        g29_led_ui_set_raw(G29_LED_3_YELLOW1); // Tam Merkez (Sarı 1)
    } else if (trim_pos == 1) {
        g29_led_ui_set_raw(G29_LED_4_YELLOW2); // Hafif Sağ Trim
    } else {
        g29_led_ui_set_raw(G29_LED_5_RED);     // Tam Sağ Trim
    }
}

void g29_led_ui_intro_animation(void) {
    // 3 kez hızlı flaş
    for (int i = 0; i < 3; i++) {
        g29_led_ui_set_raw(G29_LED_ALL);
        vTaskDelay(pdMS_TO_TICKS(70));
        g29_led_ui_clear();
        vTaskDelay(pdMS_TO_TICKS(70));
    }
}

void g29_led_ui_exit_animation(void) {
    // Dıştan içe veya soldan sağa söndürme efekti
    for (int i = 4; i >= 0; i--) {
        uint8_t mask = (1 << (i + 1)) - 1;
        g29_led_ui_set_raw(mask);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    g29_led_ui_clear();
}

