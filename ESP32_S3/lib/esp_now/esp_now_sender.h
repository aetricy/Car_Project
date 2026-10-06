#ifndef ESP_NOW_SENDER_H
#define ESP_NOW_SENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "g29_config.h"

/**
 * @brief Initializes ESP-NOW infrastructure, Wi-Fi station mode, and Core 0 transmission task.
 */
void init_esp_now_sender(void);

/**
 * @brief Safely enqueues drive telemetry to the ESP-NOW queue (Core-safe).
 * @param telemetry Pointer to telemetry packet to be transmitted
 */
void send_telemetry_to_car(const car_drive_packet_t *telemetry);

/**
 * @brief Changes active target vehicle and dynamically updates the ESP-NOW peer.
 * @param car_id Vehicle index between 0 and (CAR_MAX_COUNT - 1)
 * @return true if successful
 */
bool esp_now_sender_set_active_car(uint8_t car_id);

/**
 * @brief Returns current active vehicle ID (0-4).
 */
uint8_t esp_now_sender_get_active_car(void);

/**
 * @brief Returns whether the target vehicle is RF-linked (online).
 */
bool esp_now_sender_is_car_connected(void);

/**
 * @brief Returns true if vehicle reconnected after being offline and clears the flag.
 */
bool esp_now_sender_check_and_clear_reconnected(void);

#endif // ESP_NOW_SENDER_H