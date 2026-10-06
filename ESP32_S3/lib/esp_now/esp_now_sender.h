#ifndef ESP_NOW_SENDER_H
#define ESP_NOW_SENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "g29_config.h"

/**
 * @brief ESP-NOW altyapısını, Wi-Fi istasyon modunu ve Core 0 gönderim görevini başlatır.
 */
void init_esp_now_sender(void);

/**
 * @brief G29 telemetri paketini ESP-NOW kuyruğuna güvenli bir şekilde ekler (Core-safe).
 * @param telemetry Gönderilecek telemetri verisinin işaretçisi (pointer)
 */
void send_telemetry_to_car(const car_drive_packet_t *telemetry);

/**
 * @brief Aktif hedef aracı değiştirir ve ESP-NOW peer'ını günceller.
 * @param car_id 0 ile (CAR_MAX_COUNT - 1) arası araç indeksi
 * @return true başarılı ise
 */
bool esp_now_sender_set_active_car(uint8_t car_id);

/**
 * @brief Mevcut aktif araç ID'sini döner (0-4).
 */
uint8_t esp_now_sender_get_active_car(void);

/**
 * @brief Aracın RF seviyesinde bağlı (Online) olup olmadığını döner.
 */
bool esp_now_sender_is_car_connected(void);

/**
 * @brief Araç çevrimdışıyken ilk kez çevrimiçi olduğunda true döner ve bayrağı sıfırlar.
 */
bool esp_now_sender_check_and_clear_reconnected(void);

#endif // ESP_NOW_SENDER_H