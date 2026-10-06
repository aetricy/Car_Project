#include "pwm_control.h"
#include "driver/ledc.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

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

// RC Expo / Eğri Hesaplama Fonksiyonu
// normalized_input: 0.0 (Merkez) ile 1.0 (Tam Açı) arası
static float apply_expo(float normalized_input, uint8_t curve_type) {
    if (normalized_input < 0.0f) normalized_input = 0.0f;
    if (normalized_input > 1.0f) normalized_input = 1.0f;

    switch (curve_type) {
        case 0:
            // Lineer: Birebir doğrusal tepki
            return normalized_input;
        case 1:
            // Expo Yumuşak: Merkezde daha hassas ve yumuşak, uçlarda tam açı (Drift ve hassas kontrol için ideal)
            return 0.45f * normalized_input + 0.55f * (normalized_input * normalized_input * normalized_input);
        case 2:
            // Expo Agresif: Merkezde çok yumuşak, son çeyrekte çok hızlı açılma
            return 0.20f * normalized_input + 0.80f * (normalized_input * normalized_input * normalized_input);
        default:
            return normalized_input;
    }
}

void init_default_config(void) {
    memset(&current_config, 0, sizeof(car_config_packet_t));
    current_config.packet_type    = PKT_TYPE_CONFIG;
    
    current_config.st_epa_left    = 100;
    current_config.st_epa_right   = 100;
    current_config.st_sub_trim    = 0;
    current_config.st_reverse     = false;
    current_config.st_curve       = 0; // Lineer
    
    current_config.th_epa_forward = 100;
    current_config.th_epa_backward = 100;
    current_config.th_sub_trim    = 0;
    current_config.th_reverse     = false;
    current_config.th_curve       = 0; // Lineer
    
    current_config.st_gyro_gain   = 50; // Varsayılan %50 Gyro Gain

    RC_PRINT("[INFO] Varsayilan RC ayarlari RAM'e yuklendi.\n");
}

void update_pwm_config(const car_config_packet_t *new_config) {
    memcpy(&current_config, new_config, sizeof(car_config_packet_t));

    // Gyro Gain Dönüşümü (0% - 100% -> 1000us - 2000us)
    uint16_t gyro_us = 1500;
    if (current_config.st_gyro_gain >= 0) {
        gyro_us = 1000 + ((uint16_t)current_config.st_gyro_gain * 10);
    } else {
        gyro_us = 1500 + (current_config.st_gyro_gain * 5);
    }
    if (gyro_us < 1000) gyro_us = 1000;
    if (gyro_us > 2000) gyro_us = 2000;
    set_gyro_gain_us(gyro_us);

    RC_PRINT("\n================== YENI CONFIG ISLENDI ==================\n");
    RC_PRINT(" Direksiyon -> EPA Sol: %d%% | Sag: %d%% | Curve: %d | Trim: %d (x3) | Rev: %d\n",
             current_config.st_epa_left, current_config.st_epa_right, current_config.st_curve,
             current_config.st_sub_trim, current_config.st_reverse);
    RC_PRINT(" Gaz/Fren   -> EPA Ileri: %d%% | Geri: %d%% | Curve: %d | Trim: %d (x3) | Rev: %d\n",
             current_config.th_epa_forward, current_config.th_epa_backward, current_config.th_curve,
             current_config.th_sub_trim, current_config.th_reverse);
    RC_PRINT(" Gyro Gain  -> Gain: %d%% | PWM (GPIO%d): %d us\n",
             current_config.st_gyro_gain, GYRO_GAIN_PWM_PIN, gyro_us);
    RC_PRINT("========================================================\n\n");
}

uint16_t apply_config_to_pwm(uint16_t raw_pwm, bool is_steering) {
    if (is_steering) {
        // 1. Sürücü Girdisi (Direksiyon Yönü: Sol < 1500, Sağ > 1500)
        // raw_pwm: 1000 (Tam Sol) ... 1500 (Merkez) ... 2000 (Tam Sağ)
        float diff = (float)((int16_t)raw_pwm - 1500);
        float norm = diff / 500.0f;
        if (norm > 1.0f) norm = 1.0f;
        if (norm < -1.0f) norm = -1.0f;
        
        // Büyüklük (0.0 - 1.0) ve eğri (Curve / Expo) uygulaması
        float mag = fabsf(norm);
        float curved_mag = apply_expo(mag, current_config.st_curve);
        
        // 2. EPA Faktörü:
        // Reverse aktifken G29 sol girişi aracı SAĞA, G29 sağ girişi aracı SOLA döndürür.
        // Dolayısıyla arabanın fiziksel sol ve sağ dönüş limitleri reverse durumuna göre doğru kanala atanır:
        float factor;
        if (current_config.st_reverse) {
            // Reverse Açıkken: G29 Sol -> Sağ limit (st_epa_right), G29 Sağ -> Sol limit (st_epa_left)
            factor = (norm < 0.0f) ? (current_config.st_epa_right / 100.0f)
                                   : (current_config.st_epa_left / 100.0f);
        } else {
            // Reverse Kapalıyken: G29 Sol -> Sol limit (st_epa_left), G29 Sağ -> Sağ limit (st_epa_right)
            factor = (norm < 0.0f) ? (current_config.st_epa_left / 100.0f)
                                   : (current_config.st_epa_right / 100.0f);
        }
        
        float deflection = curved_mag * 500.0f * factor;
        
        // 3. Çıkış Yönü (Sol: -1, Sağ: +1)
        int16_t dir = (norm < 0.0f) ? -1 : 1;
        int16_t output_offset = (int16_t)(deflection * dir);
        
        // 4. Reverse (Ters Yön) Uygulaması:
        if (current_config.st_reverse) {
            output_offset = -output_offset;
        }
        
        // 5. Sub-Trim Uygulaması (Hissiyatı ve hassasiyeti artırmak için 3x çarpan)
        int16_t trim = (int16_t)current_config.st_sub_trim * 3;
        if (current_config.st_reverse) {
            trim = -trim;
        }
        
        int16_t final_pwm = 1500 + output_offset + trim;
        if (final_pwm < 1000) final_pwm = 1000;
        if (final_pwm > 2000) final_pwm = 2000;
        return (uint16_t)final_pwm;
    } else { // Gaz & Fren (Throttle)
        // 1. Sürücü Girdisi (Fren/Geri < 1500, İleri Gaz > 1500)
        float diff = (float)((int16_t)raw_pwm - 1500);
        float norm = diff / 500.0f;
        if (norm > 1.0f) norm = 1.0f;
        if (norm < -1.0f) norm = -1.0f;
        
        float mag = fabsf(norm);
        float curved_mag = apply_expo(mag, current_config.th_curve);
        
        // 2. EPA Faktörü:
        float factor;
        if (current_config.th_reverse) {
            factor = (norm < 0.0f) ? (current_config.th_epa_forward / 100.0f)
                                   : (current_config.th_epa_backward / 100.0f);
        } else {
            factor = (norm < 0.0f) ? (current_config.th_epa_backward / 100.0f)
                                   : (current_config.th_epa_forward / 100.0f);
        }
        
        float deflection = curved_mag * 500.0f * factor;
        
        // 3. Çıkış Yönü
        int16_t dir = (norm < 0.0f) ? -1 : 1;
        int16_t output_offset = (int16_t)(deflection * dir);
        
        // 4. Reverse Uygulaması
        if (current_config.th_reverse) {
            output_offset = -output_offset;
        }
        
        // 5. Sub-Trim Uygulaması (Hissiyatı ve hassasiyeti artırmak için 3x çarpan)
        int16_t trim = (int16_t)current_config.th_sub_trim * 3;
        if (current_config.th_reverse) {
            trim = -trim;
        }
        
        int16_t final_pwm = 1500 + output_offset + trim;
        if (final_pwm < 1000) final_pwm = 1000;
        if (final_pwm > 2000) final_pwm = 2000;
        return (uint16_t)final_pwm;
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

    ledc_channel_config_t ledc_channel_gyro = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_2,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GYRO_GAIN_PWM_PIN,
        .duty           = 8110, // Varsayilan %50 Gain (1500us)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_gyro));

    RC_PRINT("-> [DONANIM] PWM Baslatildi (Steering: GPIO%d | Throttle: GPIO%d | Gyro Gain: GPIO%d)\n", 
             STEERING_PWM_PIN, THROTTLE_PWM_PIN, GYRO_GAIN_PWM_PIN);
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

void set_gyro_gain_us(uint16_t raw_us) {
    if (raw_us < 1000) raw_us = 1000;
    if (raw_us > 2000) raw_us = 2000;
    uint32_t duty = (uint32_t)(((float)raw_us * 16384.0f * 330.0f) / 1000000.0f);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}