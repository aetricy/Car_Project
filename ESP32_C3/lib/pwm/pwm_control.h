#ifndef PWM_CONTROL_H
#define PWM_CONTROL_H

#include <stdint.h>

// ESP32-C3 PINOUT 
#define GYRO_SETTING_PIN  0 
#define THROTTLE_PWM_PIN  1
#define STEERING_PWM_PIN  3

// Sistem Başlatma
void init_pwm(void);

// 1000 - 2000 mikrosaniye aralığında sinyal gönderen ana fonksiyonlar
void set_steering_us(uint16_t raw_us);
void set_throttle_us(uint16_t raw_us);

#endif