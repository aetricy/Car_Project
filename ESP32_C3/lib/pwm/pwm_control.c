#include "pwm_control.h"
#include "driver/ledc.h"
#include <stdio.h>
#include <string.h>

// --- LOGLAMA MAKROSU ---
#define DEBUG_LOG_ENABLE 1  
#if DEBUG_LOG_ENABLE
    #define RC_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define RC_PRINT(fmt, ...) 
#endif

// PWM Pin Tanımları
#define STEERING_PWM_PIN 3
#define THROTTLE_PWM_PIN 1

// RAM Üzerindeki Aktif Ayarlar
static car_config_packet_t current_config;

// ==========================================
// AYAR (CONFIG) VE MATEMATİK FONKSİYONLARI
// ==========================================
void init_default_config(void) {
    memset(&current_config, 0, sizeof(car_config_packet_t));
    
    current_config.st_epa_left = 100;
    current_config.st_epa_right = 100;
    current_config.st_sub_trim = 0;
    current_config.st_reverse = false;
    
    current_config.th_epa_forward = 100;
    current_config.th_epa_backward = 100;
    current_config.th_sub_trim = 0;
    current_config.th_reverse = false;
    
    RC_PRINT("[INFO] Varsayilan RC ayarlari RAM'e yuklendi.\n");
}

void update_pwm_config(const car_config_packet_t *new_config) {
    memcpy(&current_config, new_config, sizeof(car_config_packet_t));
    RC_PRINT("[INFO] Yeni ayar paketi PWM sistemine islendi.\n");
}

uint16_t apply_config_to_pwm(uint16_t raw_pwm, bool is_steering) {
    uint16_t processed_pwm = raw_pwm;
    
    if (is_steering) {
        if (current_config.st_reverse) processed_pwm = 3000 - processed_pwm;
        
        int16_t trimmed = processed_pwm + current_config.st_sub_trim;
        
        if (trimmed < 1500) { // Sola
            float factor = current_config.st_epa_left / 100.0f;
            return 1500 - (uint16_t)((1500 - trimmed) * factor);
        } else { // Sağa
            float factor = current_config.st_epa_right / 100.0f;
            return 1500 + (uint16_t)((trimmed - 1500) * factor);
        }
    } else { // Gaz
        if (current_config.th_reverse) processed_pwm = 3000 - processed_pwm;
        
        int16_t trimmed = processed_pwm + current_config.th_sub_trim;
        
        if (trimmed < 1500) { // Geri / Fren
            float factor = current_config.th_epa_backward / 100.0f;
            return 1500 - (uint16_t)((1500 - trimmed) * factor);
        } else { // İleri
            float factor = current_config.th_epa_forward / 100.0f;
            return 1500 + (uint16_t)((trimmed - 1500) * factor);
        }
    }
}

// ==========================================
// DONANIM (PWM) FONKSİYONLARI
// ==========================================
void init_pwm(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE, 
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_14_BIT,
        .freq_hz          = 330,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel_steering = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = STEERING_PWM_PIN,
        .duty           = 8110, // 330Hz'de 1500us (Merkez)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_steering));

    ledc_channel_config_t ledc_channel_throttle = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = THROTTLE_PWM_PIN,
        .duty           = 8110, // Merkez
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_throttle));
    

    RC_PRINT("-> [DONANIM] PWM Baslatildi (Steering: GPIO%d | Throttle: GPIO%d)\n", STEERING_PWM_PIN, THROTTLE_PWM_PIN);
}

void set_steering_us(uint16_t raw_us) {
    if (raw_us < 1000) raw_us = 1000;
    if (raw_us > 2000) raw_us = 2000;
    uint32_t duty = (uint32_t)(((float)raw_us * 16384.0f * 330.0f) / 1000000.0f);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void set_throttle_us(uint16_t raw_us) {
    if (raw_us < 1000) raw_us = 1000;
    if (raw_us > 2000) raw_us = 2000;
    uint32_t duty = (uint32_t)(((float)raw_us * 16384.0f * 330.0f) / 1000000.0f);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}