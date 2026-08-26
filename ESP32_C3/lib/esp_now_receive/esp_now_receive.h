#ifndef ESP_NOW_RECEIVE_H
#define ESP_NOW_RECEIVE_H

#include <stdbool.h>
#include "g29_config.h" // g29_telemetry_t yapısı için gerekli

// Wi-Fi, NVS ve ESP-NOW altyapısını kurar ve dinlemeye başlar
void init_esp_now_receiver(void);

// Yeni veri varsa out_data içine kopyalar ve true döndürür. Yoksa false döndürür.
bool esp_now_get_latest_data(g29_telemetry_t *out_data);

#endif // ESP_NOW_RECEIVER_H