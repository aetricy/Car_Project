#ifndef CONFIG_CONTROL_H
#define CONFIG_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "g29_config.h"

// 7 Configuration Menus (Indicated via G29 Shift LEDs)
typedef enum {
    CFG_MENU_ST_EPA = 0,    // LED 1 (Green 1) : Steering EPA (Left / Right / Dual Rate)
    CFG_MENU_ST_CURVE,      // LED 2 (Green 2) : Steering Curve / Expo (0: Linear, 1: Mild Expo, 2: Aggressive Expo)
    CFG_MENU_TH_EPA,        // LED 3 (Yellow 1) : Throttle & Brake EPA (Forward / Backward Max Power)
    CFG_MENU_TH_CURVE,      // LED 4 (Yellow 2) : Throttle Curve / Expo (0: Linear, 1: Soft Launch Expo, 2: Aggressive Expo)
    CFG_MENU_ST_TRIM,       // LED 5 (Red)      : Steering Sub-Trim (Center Fine Adjustment)
    CFG_MENU_GYRO_GAIN,     // LED 1+5 (Green 1 + Red) : Drift Gyro Gain (0% - 100%, GPIO 0 PWM)
    CFG_MENU_CAR_SELECT,    // LED 2+3+4 (Middle 3 LEDs) : Vehicle Profile Select (Cars 1 - 5)
    CFG_MENU_COUNT
} cfg_menu_t;

/**
 * @brief Initializes the config controller, loading persistent settings from NVS.
 */
void config_control_init(void);

/**
 * @brief Selects active vehicle, persists to NVS, and loads corresponding settings.
 * @param car_id Vehicle index between 0 and 4
 * @return true if successful
 */
bool config_control_select_car(uint8_t car_id);

/**
 * @brief Returns current active vehicle index (0-4).
 */
uint8_t config_control_get_active_car_id(void);

/**
 * @brief Checks if Dev Mode is currently active.
 */
bool config_control_is_dev_mode(void);

/**
 * @brief Overrides Dev Mode state.
 */
void config_control_set_dev_mode(bool enable);

/**
 * @brief Processes G29 telemetry, captures button combos, and updates LEDs/configs.
 * @param telemetry Real-time G29 telemetry
 * @param out_config_packet Populated with new config if settings changed
 * @return true if config changed and needs ESP-NOW transmission
 */
bool config_control_process(const g29_telemetry_t *telemetry, car_config_packet_t *out_config_packet);

/**
 * @brief Returns pointer to current active configuration.
 */
const car_config_packet_t* config_control_get_active_config(void);

/**
 * @brief Persists active configuration to S3 NVS (protected with memcmp).
 */
bool config_control_save_to_nvs(void);

/**
 * @brief Loads stored configuration from S3 NVS.
 */
bool config_control_load_from_nvs(void);

#endif // CONFIG_CONTROL_H
