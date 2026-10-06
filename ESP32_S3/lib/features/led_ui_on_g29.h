#ifndef LED_UI_ON_G29_H
#define LED_UI_ON_G29_H

#include <stdint.h>
#include <stdbool.h>

// G29 RPM LED Bitmask Değerleri
// 5 Adet LED: Yeşil 1, Yeşil 2, Sarı 1, Sarı 2, Kırmızı
#define G29_LED_1_GREEN1   (1 << 0) // 0x01
#define G29_LED_2_GREEN2   (1 << 1) // 0x02
#define G29_LED_3_YELLOW1  (1 << 2) // 0x04
#define G29_LED_4_YELLOW2  (1 << 3) // 0x08
#define G29_LED_5_RED      (1 << 4) // 0x10
#define G29_LED_ALL        (0x1F)   // 31 (Hepsi açık)
#define G29_LED_NONE       (0x00)   // Hepsi kapalı

/**
 * @brief Doğrudan LED maskesini G29 direksiyonuna gönderir.
 * @param mask 0x00 - 0x1F arası bitmask
 */
void g29_led_ui_set_raw(uint8_t mask);

/**
 * @brief Tüm LED'leri kapatır.
 */
void g29_led_ui_clear(void);

/**
 * @brief Seviye göstergesi (1-5 arası bar grafiği: VU metre).
 * @param level 0 ile 5 arası değer (Örn: EPA yüzdesi için)
 */
void g29_led_ui_show_bar(uint8_t level);

/**
 * @brief Aktif menüyü tek bir LED ile gösterir (Opsiyonel yanıp sönme).
 * @param menu_index 0: LED1, 1: LED2, 2: LED3, 3: LED4, 4: LED5
 * @param is_on LED yansın mı sönsün mü (yanıp sönme efekti için)
 */
void g29_led_ui_show_menu_single(uint8_t menu_index, bool is_on);

/**
 * @brief Sub-trim merkezleme göstergesi (-2 sol, -1 hafif sol, 0 merkez, +1 hafif sağ, +2 sağ).
 * @param trim_pos -2 ile +2 arası
 */
void g29_led_ui_show_trim_position(int8_t trim_pos);

/**
 * @brief Dev Mode'a giriş animasyonu oynatır (hızlı flaş).
 */
void g29_led_ui_intro_animation(void);

/**
 * @brief Dev Mode'dan çıkış animasyonu oynatır (söndürme).
 */
void g29_led_ui_exit_animation(void);

#endif // LED_UI_ON_G29_H

