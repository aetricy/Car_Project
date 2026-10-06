#ifndef G29_DRIVER_HOST_H
#define G29_DRIVER_HOST_H

#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.h"

// G29'un anlık durumlarını belirten Enum
typedef enum {
    G29_STATE_DISCONNECTED,
    G29_STATE_PS3_WAKING_UP,
    G29_STATE_NATIVE_READY
} g29_state_t;

// Kullanıcı için Callback fonksiyon tipleri
typedef void (*g29_state_callback_t)(g29_state_t state);
typedef void (*g29_input_callback_t)(const uint8_t *data, int len);

// ---- API FONKSİYONLARI ----

/**
 * @brief G29 USB sürücüsünü arka planda başlatır.
 * @param state_cb Durum değişimlerinde (Takıldı, Hazır, Çıktı) tetiklenir.
 * @param input_cb Direksiyondan veri geldiğinde tetiklenir.
 */
bool g29_init(g29_state_callback_t state_cb, g29_input_callback_t input_cb);

/**
 * @brief Cihazın Native modda çalışmaya hazır olup olmadığını döner.
 */
bool g29_is_ready(void);

/**
 * @brief Direksiyon üzerindeki devir LED'lerini yakar.
 * @param val LED değeri (0 kapalı, 31 hepsi açık)
 */
void g29_set_leds(uint8_t val);

/**
 * @brief Direksiyonun maksimum dönme açısını (Range) ayarlar.
 * @param range Açı değeri (Örn: 900 tam tur, F1 için 360, RC Drift için 540)
 */
void g29_set_range(uint16_t range);

/**
 * @brief Direksiyona sürekli bir tork (güç) uygular.
 * @param force -1.0 (Tam Sol) ile 1.0 (Tam Sağ) arası.
 */
void g29_set_constant_force(float force);

/**
 * @brief Direksiyona mekanik bir sürtünme (ağırlık) ekler (Non-blocking).
 * @param friction 0.0 (Hafif/Sürtünme yok) ile 1.0 (Maksimum ağırlık/Sert) arası.
 */
void g29_set_friction(float friction);

/**
 * @brief Mekanik sürtünme değerini doğrudan donanım kademesi ile ayarlar (Non-blocking).
 * @param f_val Sürtünme kademesi (0 - 7 arası)
 */
void g29_set_friction_raw(uint8_t f_val);

/**
 * @brief G29 donanımsal otomatik merkezleme modunu başlatır (0x14 komutu).
 */
void g29_enable_autocenter(void);

/**
 * @brief G29'un kendi donanımsal otomatik merkezleme yayını tamamen iptal eder (0xF5 komutu).
 */
void g29_disable_autocenter(void);

/**
 * @brief Tüm aktif motor torklarını ve efektlerini anında durdurur (Acil Stop).
 */
void g29_force_off(void);

/**
 * @brief Donanımsal merkezleme yay gücünü ve hızını ham değerlerle günceller (Non-blocking).
 * @param s_val Güç kademesi (0 - 15)
 * @param r_val Hız/Eğim kademesi (0 - 255)
 */
void g29_set_autocenter_raw(uint8_t s_val, uint8_t r_val);

/**
 * @brief Direksiyonun kendini ortalama (Autocenter) gücünü ve hızını ayarlar.
 * @param strength Merkezleme gücü (0.0 ile 1.0 arası)
 * @param rate Merkezleme hızı (0.0 ile 1.0 arası)
 */
void g29_set_autocenter(float strength, float rate);

/**
 * @brief Gelecekte eklenecek Force Feedback efektleri için şablon fonksiyon
 */
void g29_send_ffb_command(const uint8_t *command, size_t len);

#endif // G29_DRIVER_HOST_H