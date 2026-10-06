#ifndef PWM_CONTROL_H
#define PWM_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "g29_config.h"

// --- Ayar (Config) ve Matematik Fonksiyonları ---
void init_default_config(void);
void update_pwm_config(const car_config_packet_t *new_config);
uint16_t apply_config_to_pwm(uint16_t raw_pwm, bool is_steering);

// --- Donanım (PWM) Fonksiyonları ---
#define STEERING_PWM_PIN    3
#define THROTTLE_PWM_PIN    1
#define GYRO_GAIN_PWM_PIN   0

void init_pwm(void);
void set_steering_us(uint16_t raw_us);
void set_throttle_us(uint16_t raw_us);
void set_gyro_gain_us(uint16_t raw_us);

#endif // PWM_CONTROL_H