#include "pwm_control.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// --- LOGGING MACRO ---
#define DEBUG_LOG_ENABLE 1  
#if DEBUG_LOG_ENABLE
    #define RC_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define RC_PRINT(fmt, ...) 
#endif

// PWM Pin Definitions
#define STEERING_PWM_PIN 3
#define THROTTLE_PWM_PIN 1

// NVS Namespace and Key
#define NVS_NAMESPACE_CAR_CFG "car_cfg_ns"
#define NVS_KEY_CAR_CFG       "cfg_blob"

// Active Configuration Cached in RAM
static car_config_packet_t current_config;

// RC Expo / Curve Calculation Function
// normalized_input: 0.0 (Center) to 1.0 (Full throw)
static float apply_expo(float normalized_input, uint8_t curve_type) {
    if (normalized_input < 0.0f) normalized_input = 0.0f;
    if (normalized_input > 1.0f) normalized_input = 1.0f;

    switch (curve_type) {
        case 0:
            // Linear: 1:1 direct response
            return normalized_input;
        case 1:
            // Expo Soft: Softer near center, progressive towards extremes (ideal for drift control)
            return 0.45f * normalized_input + 0.55f * (normalized_input * normalized_input * normalized_input);
        case 2:
            // Expo Aggressive: Very soft near center, ramps up quickly in outer throw
            return 0.20f * normalized_input + 0.80f * (normalized_input * normalized_input * normalized_input);
        default:
            return normalized_input;
    }
}

// Configuration validation (guard against corrupted flash or unformatted NVS)
static bool is_config_valid(const car_config_packet_t *cfg) {
    if (!cfg) return false;
    if (cfg->st_epa_left < 20 || cfg->st_epa_left > 100) return false;
    if (cfg->st_epa_right < 20 || cfg->st_epa_right > 100) return false;
    if (cfg->st_curve > 2) return false;
    if (cfg->st_sub_trim < -50 || cfg->st_sub_trim > 50) return false;
    if (cfg->th_epa_forward < 20 || cfg->th_epa_forward > 100) return false;
    if (cfg->th_epa_backward < 20 || cfg->th_epa_backward > 100) return false;
    if (cfg->th_curve > 2) return false;
    if (cfg->th_sub_trim < -50 || cfg->th_sub_trim > 50) return false;
    if (cfg->st_gyro_gain < 0 || cfg->st_gyro_gain > 100) return false;
    return true;
}

void init_default_config(void) {
    memset(&current_config, 0, sizeof(car_config_packet_t));
    current_config.packet_type    = PKT_TYPE_CONFIG;
    
    current_config.st_epa_left    = 100;
    current_config.st_epa_right   = 100;
    current_config.st_sub_trim    = 0;
    current_config.st_reverse     = false;
    current_config.st_curve       = 0; // Linear
    
    current_config.th_epa_forward = 100;
    current_config.th_epa_backward = 100;
    current_config.th_sub_trim    = 0;
    current_config.th_reverse     = false;
    current_config.th_curve       = 0; // Linear
    
    current_config.st_gyro_gain   = 50; // Default 50% Gyro Gain

    RC_PRINT("[INFO] Default RC configuration loaded into RAM.\n");
}

bool load_config_from_nvs(car_config_packet_t *out_config) {
    if (!out_config) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_CAR_CFG, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    car_config_packet_t loaded;
    size_t req_len = sizeof(car_config_packet_t);
    err = nvs_get_blob(handle, NVS_KEY_CAR_CFG, &loaded, &req_len);
    nvs_close(handle);

    if (err == ESP_OK && req_len == sizeof(car_config_packet_t) && is_config_valid(&loaded)) {
        memcpy(out_config, &loaded, sizeof(car_config_packet_t));
        return true;
    }

    return false;
}

bool save_config_to_nvs(const car_config_packet_t *new_config) {
    if (!new_config) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_CAR_CFG, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        RC_PRINT("[NVS ERROR] Failed to open NVS: %s\n", esp_err_to_name(err));
        return false;
    }

    // Flash wear leveling: Read existing data and compare before writing
    car_config_packet_t existing_cfg;
    size_t req_len = sizeof(car_config_packet_t);
    err = nvs_get_blob(handle, NVS_KEY_CAR_CFG, &existing_cfg, &req_len);
    if (err == ESP_OK && req_len == sizeof(car_config_packet_t)) {
        if (memcmp(&existing_cfg, new_config, sizeof(car_config_packet_t)) == 0) {
            // Data identical, skip flash write (wear leveling protection)
            nvs_close(handle);
            RC_PRINT("[NVS] Data matches existing NVS blob, flash write skipped (wear preserved).\n");
            return true;
        }
    }

    err = nvs_set_blob(handle, NVS_KEY_CAR_CFG, new_config, sizeof(car_config_packet_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            RC_PRINT("[NVS] RC configuration written to Flash NVS and committed successfully!\n");
        } else {
            RC_PRINT("[NVS ERROR] Commit failed: %s\n", esp_err_to_name(err));
        }
    } else {
        RC_PRINT("[NVS ERROR] Set blob failed: %s\n", esp_err_to_name(err));
    }

    nvs_close(handle);
    return (err == ESP_OK);
}

void init_config_with_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (load_config_from_nvs(&current_config)) {
        RC_PRINT("[NVS] Stored vehicle configuration loaded from NVS! (EPA_L: %d%%, EPA_R: %d%%, Gyro: %d%%, Trim: %d)\n",
                 current_config.st_epa_left, current_config.st_epa_right, current_config.st_gyro_gain, current_config.st_sub_trim);
        // Initialize Gyro gain PWM output from loaded config
        uint16_t gyro_us = 1000 + ((uint16_t)current_config.st_gyro_gain * 10);
        set_gyro_gain_us(gyro_us);
    } else {
        RC_PRINT("[NVS] Valid configuration not found, applying defaults and writing to NVS.\n");
        init_default_config();
        save_config_to_nvs(&current_config);
        set_gyro_gain_us(1500);
    }
}

const car_config_packet_t* get_current_config(void) {
    return &current_config;
}

void update_pwm_config(const car_config_packet_t *new_config) {
    memcpy(&current_config, new_config, sizeof(car_config_packet_t));

    // Gyro Gain conversion (0% - 100% -> 1000us - 2000us)
    uint16_t gyro_us = 1500;
    if (current_config.st_gyro_gain >= 0) {
        gyro_us = 1000 + ((uint16_t)current_config.st_gyro_gain * 10);
    } else {
        gyro_us = 1500 + (current_config.st_gyro_gain * 5);
    }
    if (gyro_us < 1000) gyro_us = 1000;
    if (gyro_us > 2000) gyro_us = 2000;
    set_gyro_gain_us(gyro_us);

    RC_PRINT("\n================== NEW CONFIG APPLIED ==================\n");
    RC_PRINT(" Steering -> EPA Left: %d%% | Right: %d%% | Curve: %d | Trim: %d (x3) | Rev: %d\n",
             current_config.st_epa_left, current_config.st_epa_right, current_config.st_curve,
             current_config.st_sub_trim, current_config.st_reverse);
    RC_PRINT(" Throttle -> EPA Fwd: %d%% | Rev: %d%% | Curve: %d | Trim: %d (x3) | Rev: %d\n",
             current_config.th_epa_forward, current_config.th_epa_backward, current_config.th_curve,
             current_config.th_sub_trim, current_config.th_reverse);
    RC_PRINT(" Gyro Gain  -> Gain: %d%% | PWM (GPIO%d): %d us\n",
             current_config.st_gyro_gain, GYRO_GAIN_PWM_PIN, gyro_us);
    RC_PRINT("========================================================\n\n");
}

uint16_t apply_config_to_pwm(uint16_t raw_pwm, bool is_steering) {
    if (is_steering) {
        // 1. Driver Input (Steering Direction: Left < 1500, Right > 1500)
        // raw_pwm: 1000 (Full Left) ... 1500 (Center) ... 2000 (Full Right)
        float diff = (float)((int16_t)raw_pwm - 1500);
        float norm = diff / 500.0f;
        if (norm > 1.0f) norm = 1.0f;
        if (norm < -1.0f) norm = -1.0f;
        
        // Magnitude (0.0 - 1.0) and Curve / Expo application
        float mag = fabsf(norm);
        float curved_mag = apply_expo(mag, current_config.st_curve);
        
        // 2. EPA Scaling:
        // When reverse is enabled, G29 left input turns car RIGHT, and G29 right turns car LEFT.
        // Therefore physical limits must be mapped to the corresponding direction:
        float factor;
        if (current_config.st_reverse) {
            // Reverse Enabled: G29 Left -> Right limit (st_epa_right), G29 Right -> Left limit (st_epa_left)
            factor = (norm < 0.0f) ? (current_config.st_epa_right / 100.0f)
                                   : (current_config.st_epa_left / 100.0f);
        } else {
            // Reverse Disabled: G29 Left -> Left limit (st_epa_left), G29 Right -> Right limit (st_epa_right)
            factor = (norm < 0.0f) ? (current_config.st_epa_left / 100.0f)
                                   : (current_config.st_epa_right / 100.0f);
        }
        
        float deflection = curved_mag * 500.0f * factor;
        
        // 3. Output Direction (Left: -1, Right: +1)
        int16_t dir = (norm < 0.0f) ? -1 : 1;
        int16_t output_offset = (int16_t)(deflection * dir);
        
        // 4. Invert Output if Reversed:
        if (current_config.st_reverse) {
            output_offset = -output_offset;
        }
        
        // 5. Apply Sub-Trim (3x multiplier for enhanced range and resolution)
        int16_t trim = (int16_t)current_config.st_sub_trim * 3;
        if (current_config.st_reverse) {
            trim = -trim;
        }
        
        int16_t final_pwm = 1500 + output_offset + trim;
        if (final_pwm < 1000) final_pwm = 1000;
        if (final_pwm > 2000) final_pwm = 2000;
        return (uint16_t)final_pwm;
    } else { // Throttle & Brake
        // 1. Driver Input (Brake/Reverse < 1500, Forward Throttle > 1500)
        float diff = (float)((int16_t)raw_pwm - 1500);
        float norm = diff / 500.0f;
        if (norm > 1.0f) norm = 1.0f;
        if (norm < -1.0f) norm = -1.0f;
        
        float mag = fabsf(norm);
        float curved_mag = apply_expo(mag, current_config.th_curve);
        
        // 2. EPA Scaling:
        float factor;
        if (current_config.th_reverse) {
            factor = (norm < 0.0f) ? (current_config.th_epa_forward / 100.0f)
                                   : (current_config.th_epa_backward / 100.0f);
        } else {
            factor = (norm < 0.0f) ? (current_config.th_epa_backward / 100.0f)
                                   : (current_config.th_epa_forward / 100.0f);
        }
        
        float deflection = curved_mag * 500.0f * factor;
        
        // 3. Output Direction
        int16_t dir = (norm < 0.0f) ? -1 : 1;
        int16_t output_offset = (int16_t)(deflection * dir);
        
        // 4. Invert Output if Reversed
        if (current_config.th_reverse) {
            output_offset = -output_offset;
        }
        
        // 5. Apply Sub-Trim (3x multiplier for enhanced range and resolution)
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
// HARDWARE (PWM) FUNCTIONS
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
        .duty           = 8110, // 1500us at 330Hz (Center)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_steering));

    ledc_channel_config_t ledc_channel_throttle = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = THROTTLE_PWM_PIN,
        .duty           = 8110, // Center
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_throttle));

    ledc_channel_config_t ledc_channel_gyro = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_2,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GYRO_GAIN_PWM_PIN,
        .duty           = 8110, // Default 50% Gain (1500us)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_gyro));

    RC_PRINT("-> [HARDWARE] PWM Initialized (Steering: GPIO%d | Throttle: GPIO%d | Gyro Gain: GPIO%d)\n", 
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