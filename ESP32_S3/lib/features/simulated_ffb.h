#ifndef SIMULATED_FFB_H
#define SIMULATED_FFB_H

#include <stdbool.h>
#include <stdint.h>
#include "CONFIG.h"
#include "g29_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Simüle edilmiş Force Feedback (FFB) sistemini başlatır.
 *        Park modu profilini (hafif sert sürtünme ve nazik merkezleme) direksiyona uygular.
 */
void simulated_ffb_init(void);

/**
 * @brief Direksiyon ve pedal telemetrisini işleyerek dinamik araç fizik simülasyonunu günceller.
 *        Hız tahmini, viraj kaster merkezleme torku, önden kayma (understeer) ve fren yük transferini
 *        hesaplayıp G29'a non-blocking olarak aktarır.
 *        Logic_Task döngüsünde periyodik olarak çağrılmalıdır.
 * @param telemetry G29'dan okunan güncel telemetri verisi
 */
void simulated_ffb_update(const g29_telemetry_t *telemetry);

/**
 * @brief FFB sistemini etkinleştirir veya devre dışı bırakır.
 *        Uyku modunda veya acil durumlarda motor akımını kesmek için false verilir.
 * @param enabled true: aktif simülasyon, false: motorlar serbest
 */
void simulated_ffb_set_enabled(bool enabled);

/**
 * @brief FFB simülasyonunun anlık açık/kapalı durumunu döner.
 */
bool simulated_ffb_is_enabled(void);

/**
 * @brief FFB simülasyonunu açıp kapatır (Açıksa kapatır, kapalıysa açar).
 * @return Yeni durum (true: aktif, false: kapalı/serbest)
 */
bool simulated_ffb_toggle(void);

/**
 * @brief Tahmini araç hızını ve simülasyon durumunu sıfırlar.
 *        Araç değişiminde, uykudan uyanmada veya failsafe anında park durumuna döndürür.
 */
void simulated_ffb_reset(void);

/**
 * @brief Anlık simüle edilen araç hızını döner (0.0f durmuş, 1.0f tam hız).
 */
float simulated_ffb_get_estimated_speed(void);

#ifdef __cplusplus
}
#endif

#endif // SIMULATED_FFB_H

