#ifndef LED_UI_ON_G29_H
#define LED_UI_ON_G29_H

#include <stdint.h>
#include <stdbool.h>

// G29 RPM LED Bitmask Values
// 5 LEDs: Green 1, Green 2, Yellow 1, Yellow 2, Red
#define G29_LED_1_GREEN1   (1 << 0) // 0x01
#define G29_LED_2_GREEN2   (1 << 1) // 0x02
#define G29_LED_3_YELLOW1  (1 << 2) // 0x04
#define G29_LED_4_YELLOW2  (1 << 3) // 0x08
#define G29_LED_5_RED      (1 << 4) // 0x10
#define G29_LED_ALL        (0x1F)   // 31 (All on)
#define G29_LED_NONE       (0x00)   // All off

/**
 * @brief Sends raw LED bitmask directly to the G29 steering wheel.
 * @param mask Bitmask from 0x00 to 0x1F
 */
void g29_led_ui_set_raw(uint8_t mask);

/**
 * @brief Turns off all LEDs.
 */
void g29_led_ui_clear(void);

/**
 * @brief Level indicator bar (1-5 progressive bar graph VU meter).
 * @param level Value between 0 and 5 (e.g. for EPA percentages)
 */
void g29_led_ui_show_bar(uint8_t level);

/**
 * @brief Indicates active menu using a single LED (optional blinking).
 * @param menu_index 0: LED1, 1: LED2, 2: LED3, 3: LED4, 4: LED5
 * @param is_on Whether LED should be on or off (for blinking effect)
 */
void g29_led_ui_show_menu_single(uint8_t menu_index, bool is_on);

/**
 * @brief Sub-trim centering indicator (-2 full left, -1 mid-left, 0 center, +1 mid-right, +2 full right).
 * @param trim_pos Position between -2 and +2
 */
void g29_led_ui_show_trim_position(int8_t trim_pos);

/**
 * @brief Plays Dev Mode entry animation (quick flash).
 */
void g29_led_ui_intro_animation(void);

/**
 * @brief Plays Dev Mode exit animation (sweep off).
 */
void g29_led_ui_exit_animation(void);

/**
 * @brief Synchronizes RPM shift LEDs with throttle input during driving mode.
 *        At 0% throttle, all LEDs are off; as throttle increases, LEDs light up green -> yellow -> red.
 *        At full throttle (96%+), triggers shift-light limiter blinking effect.
 * @param throttle Normalized throttle value from 0.0 to 1.0
 */
void g29_led_ui_update_throttle(float throttle);

/**
 * @brief Resets the throttle LED cache (ensures immediate update upon menu transition).
 */
void g29_led_ui_reset_throttle_cache(void);

#endif // LED_UI_ON_G29_H

