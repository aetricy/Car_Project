#ifndef ESP_NOW_RECEIVE_H
#define ESP_NOW_RECEIVE_H

#include <stdbool.h>
#include "g29_config.h" // Shared packet structures

// ==========================================
// FUNCTION PROTOTYPES
// ==========================================
void init_esp_now_receiver(void);

// Retrieves drive telemetry data
bool esp_now_get_latest_data(car_drive_packet_t *out_data);

// Retrieves command and configuration packet data
bool esp_now_get_command_data(car_command_packet_t *out_data);
bool esp_now_get_config_data(car_config_packet_t *out_data);

#endif // ESP_NOW_RECEIVE_H