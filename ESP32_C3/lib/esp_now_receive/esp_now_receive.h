#ifndef ESP_NOW_RECEIVE_H
#define ESP_NOW_RECEIVE_H

#include <stdbool.h>
#include "g29_config.h" // car_command_packet_t vb. structların olduğu dosya



// ==========================================
// FONKSİYON PROTOTİPLERİ
// ==========================================
void init_esp_now_receiver(void);

// Sürüş verisini çeken fonksiyon (Eski)
bool esp_now_get_latest_data(car_drive_packet_t *out_data);

// Komut ve Ayar verilerini çeken YENİ fonksiyonlar
bool esp_now_get_command_data(car_command_packet_t *out_data);
bool esp_now_get_config_data(car_config_packet_t *out_data);

#endif // ESP_NOW_RECEIVE_H