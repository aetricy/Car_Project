#ifndef ESP_NOW_SENDER_H
#define ESP_NOW_SENDER_H

#include "g29_config.h" // g29_telemetry_t struct tanımı için gerekli

/**
 * @brief ESP-NOW altyapısını, Wi-Fi istasyon modunu ve Core 0 gönderim görevini başlatır.
 */
void init_esp_now_sender(void);

/**
 * @brief G29 telemetri paketini ESP-NOW kuyruğuna güvenli bir şekilde ekler (Core-safe).
 * @param telemetry Gönderilecek telemetri verisinin işaretçisi (pointer)
 */
void send_telemetry_to_car(const g29_telemetry_t *telemetry);

#endif // ESP_NOW_SENDER_H