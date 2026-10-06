#ifndef CONFIG_CONTROL_H
#define CONFIG_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "g29_config.h"

// 6 Parametre Menüsü (G29 RPM LED'leri ile gösterilir)
typedef enum {
    CFG_MENU_ST_EPA = 0,    // LED 1 (Yeşil 1) : Direksiyon EPA (Sol / Sağ / Dual Rate)
    CFG_MENU_ST_CURVE,      // LED 2 (Yeşil 2) : Direksiyon Eğrisi / Expo (0: Lineer, 1: Yumuşak Expo, 2: Agresif Expo)
    CFG_MENU_TH_EPA,        // LED 3 (Sarı 1)  : Gaz & Fren EPA (İleri / Geri Maksimum Güç)
    CFG_MENU_TH_CURVE,      // LED 4 (Sarı 2)  : Gaz Eğrisi / Expo (0: Lineer, 1: Yumuşak Expo, 2: Agresif Expo)
    CFG_MENU_ST_TRIM,       // LED 5 (Kırmızı) : Direksiyon Sub-Trim (Merkez İnce Ayarı)
    CFG_MENU_GYRO_GAIN,     // LED 1+5 (Yeşil 1 + Kırmızı) : Gyro Gain (%0 - %100, GPIO 0 PWM)
    CFG_MENU_COUNT
} cfg_menu_t;

/**
 * @brief Config kontrolcüsünü varsayılan ayarlarla başlatır.
 */
void config_control_init(void);

/**
 * @brief Dev Mode aktif mi sorgular.
 */
bool config_control_is_dev_mode(void);

/**
 * @brief Dev Mode durumunu zorla değiştirir.
 */
void config_control_set_dev_mode(bool enable);

/**
 * @brief G29 telemetrisini işler, tuş kombinasyonlarını yakalar ve LED'leri günceller.
 * @param telemetry G29'dan gelen anlık telemetri
 * @param out_config_packet Eğer ayar değiştiyse doldurulacak paket yapısı
 * @return true Eğer ayar değiştiyse ve ESP-NOW ile C3'e gönderilmesi gerekiyorsa
 */
bool config_control_process(const g29_telemetry_t *telemetry, car_config_packet_t *out_config_packet);

/**
 * @brief Güncel aktif konfigürasyon işaretçisini döner.
 */
const car_config_packet_t* config_control_get_active_config(void);

#endif // CONFIG_CONTROL_H

