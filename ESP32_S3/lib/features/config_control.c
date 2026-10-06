#include "config_control.h"
#include "led_ui_on_g29.h"
#include "g29_button_map.h"
#include "CONFIG.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_now_sender.h"
#include "simulated_ffb.h"

static const char *TAG = "DEV_CONFIG";

#define NVS_NAMESPACE_S3_CFG "s3_cfg_ns"
#define NVS_KEY_ACTIVE_CAR   "car_id"

// Active configuration data and active vehicle index (0-4)
static car_config_packet_t g_active_config;
static uint8_t s_active_car_id = 0;

static void get_car_cfg_key(uint8_t car_id, char *out_key) {
    snprintf(out_key, 16, "cfg_car_%u", (unsigned int)car_id);
}

// Dev Mode State
static bool s_dev_mode_active = false;
static cfg_menu_t s_current_menu = CFG_MENU_ST_EPA;

// NVS Wear Leveling / Flash Protection Variables
static bool s_config_dirty = false;
static TickType_t s_last_config_change_tick = 0;

// Timers and Button States
static TickType_t s_value_display_until = 0;
static uint32_t s_last_buttons = 0;
static uint32_t s_combo_hold_ticks = 0;
static bool s_combo_latched = false;
static uint32_t s_enter_hold_ticks = 0;
static bool s_enter_latched = false;

extern volatile s3_logic_state_t current_system_state;
extern QueueHandle_t espnow_tx_queue;

static void send_save_cmd_to_car(void) {
    if (espnow_tx_queue != NULL) {
        espnow_tx_item_t cmd_item;
        memset(&cmd_item, 0, sizeof(espnow_tx_item_t));
        cmd_item.length = sizeof(car_command_packet_t);
        cmd_item.payload.command.packet_type = PKT_TYPE_COMMAND;
        cmd_item.payload.command.command_id  = CMD_CONFIG_SAVE;
        cmd_item.payload.command.parameter   = 0;
        xQueueSend(espnow_tx_queue, &cmd_item, 0);
        ESP_LOGI(TAG, "CMD_CONFIG_SAVE command sent to car.");
    }
}

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

bool config_control_save_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_S3_CFG, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open S3 NVS: %s", esp_err_to_name(err));
        return false;
    }

    // 1. Save active vehicle ID
    nvs_set_u8(handle, NVS_KEY_ACTIVE_CAR, s_active_car_id);

    // 2. Save config for this vehicle (with memcmp check to prevent redundant writes)
    char key[16];
    get_car_cfg_key(s_active_car_id, key);

    car_config_packet_t existing_cfg;
    size_t req_len = sizeof(car_config_packet_t);
    err = nvs_get_blob(handle, key, &existing_cfg, &req_len);
    if (err == ESP_OK && req_len == sizeof(car_config_packet_t)) {
        if (memcmp(&existing_cfg, &g_active_config, sizeof(car_config_packet_t)) == 0) {
            nvs_commit(handle);
            nvs_close(handle);
            ESP_LOGI(TAG, "S3 NVS: [Vehicle %d] data identical, flash write skipped.", s_active_car_id + 1);
            return true;
        }
    }

    err = nvs_set_blob(handle, key, &g_active_config, sizeof(car_config_packet_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "S3 NVS: [Vehicle %d] config successfully saved to Flash NVS.", s_active_car_id + 1);
        } else {
            ESP_LOGE(TAG, "S3 NVS commit error: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "S3 NVS set_blob error: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    return (err == ESP_OK);
}

bool config_control_load_from_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_S3_CFG, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    // 1. Read stored active vehicle ID
    uint8_t saved_car_id = 0;
    if (nvs_get_u8(handle, NVS_KEY_ACTIVE_CAR, &saved_car_id) == ESP_OK && saved_car_id < CAR_MAX_COUNT) {
        s_active_car_id = saved_car_id;
    } else {
        s_active_car_id = 0;
    }

    // 2. Read config for this vehicle
    char key[16];
    get_car_cfg_key(s_active_car_id, key);

    car_config_packet_t loaded;
    size_t req_len = sizeof(car_config_packet_t);
    err = nvs_get_blob(handle, key, &loaded, &req_len);
    nvs_close(handle);

    // Notify sender of active vehicle
    esp_now_sender_set_active_car(s_active_car_id);

    if (err == ESP_OK && req_len == sizeof(car_config_packet_t) && is_config_valid(&loaded)) {
        memcpy(&g_active_config, &loaded, sizeof(car_config_packet_t));
        return true;
    }

    return false;
}

bool config_control_select_car(uint8_t new_car_id) {
    if (new_car_id >= CAR_MAX_COUNT) return false;
    if (new_car_id == s_active_car_id) return true;

    // Save pending changes of previous vehicle if any
    if (s_config_dirty) {
        config_control_save_to_nvs();
        s_config_dirty = false;
    }

    s_active_car_id = new_car_id;
    esp_now_sender_set_active_car(new_car_id);

    // Load new vehicle NVS configuration if available, otherwise set defaults
    nvs_handle_t handle;
    bool loaded = false;
    if (nvs_open(NVS_NAMESPACE_S3_CFG, NVS_READONLY, &handle) == ESP_OK) {
        char key[16];
        get_car_cfg_key(new_car_id, key);
        car_config_packet_t cfg_loaded;
        size_t len = sizeof(car_config_packet_t);
        if (nvs_get_blob(handle, key, &cfg_loaded, &len) == ESP_OK && len == sizeof(car_config_packet_t) && is_config_valid(&cfg_loaded)) {
            memcpy(&g_active_config, &cfg_loaded, sizeof(car_config_packet_t));
            loaded = true;
        }
        nvs_close(handle);
    }

    if (!loaded) {
        g_active_config.packet_type     = PKT_TYPE_CONFIG;
        g_active_config.st_gyro_gain    = 50;
        g_active_config.st_sub_trim     = 0;
        g_active_config.st_epa_left     = 100;
        g_active_config.st_epa_right    = 100;
        g_active_config.st_reverse      = false;
        g_active_config.st_curve        = 0;
        g_active_config.th_sub_trim     = 0;
        g_active_config.th_epa_forward  = 100;
        g_active_config.th_epa_backward = 100;
        g_active_config.th_reverse      = false;
        g_active_config.th_curve        = 0;
    }

    // Persist selected vehicle and configuration to NVS
    config_control_save_to_nvs();

    // Send configuration and wake-up packet to new vehicle
    if (espnow_tx_queue != NULL) {
        espnow_tx_item_t cfg_item;
        memset(&cfg_item, 0, sizeof(espnow_tx_item_t));
        cfg_item.length = sizeof(car_config_packet_t);
        cfg_item.payload.config = g_active_config;
        xQueueSend(espnow_tx_queue, &cfg_item, 0);

        espnow_tx_item_t wake_item;
        memset(&wake_item, 0, sizeof(espnow_tx_item_t));
        wake_item.length = sizeof(car_command_packet_t);
        wake_item.payload.command.packet_type = PKT_TYPE_COMMAND;
        wake_item.payload.command.command_id  = CMD_WAKE_UP;
        wake_item.payload.command.parameter   = 0;
        xQueueSend(espnow_tx_queue, &wake_item, 0);
    }

    ESP_LOGW(TAG, ">>> ACTIVE VEHICLE SWITCHED -> [VEHICLE %d] <<<", new_car_id + 1);
    simulated_ffb_reset();
    return true;
}

uint8_t config_control_get_active_car_id(void) {
    return s_active_car_id;
}

void config_control_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (config_control_load_from_nvs()) {
        ESP_LOGI(TAG, "S3 NVS: [Vehicle %d] settings successfully loaded! (EPA Left: %d%%, Right: %d%%, Gyro: %d%%, Trim: %d)",
                 s_active_car_id + 1,
                 g_active_config.st_epa_left, g_active_config.st_epa_right, g_active_config.st_gyro_gain, g_active_config.st_sub_trim);
    } else {
        s_active_car_id = 0;
        g_active_config.packet_type     = PKT_TYPE_CONFIG;
        g_active_config.st_gyro_gain    = 50; // Default 50% Gain (1500us)
        
        // Steering Defaults
        g_active_config.st_sub_trim     = 0;
        g_active_config.st_epa_left     = 100;
        g_active_config.st_epa_right    = 100;
        g_active_config.st_reverse      = false;
        g_active_config.st_curve        = 0; // 0: Linear
        
        // Throttle Defaults
        g_active_config.th_sub_trim     = 0;
        g_active_config.th_epa_forward  = 100;
        g_active_config.th_epa_backward = 100;
        g_active_config.th_reverse      = false;
        g_active_config.th_curve        = 0; // 0: Linear

        config_control_save_to_nvs();
        esp_now_sender_set_active_car(s_active_car_id);
        ESP_LOGI(TAG, "Config Manager initialized (Default Vehicle 1 applied and saved to NVS).");
    }

    s_dev_mode_active = false;
    s_current_menu = CFG_MENU_ST_EPA;
    s_value_display_until = 0;
    s_combo_hold_ticks = 0;
    s_combo_latched = false;
    s_config_dirty = false;
}

bool config_control_is_dev_mode(void) {
    return s_dev_mode_active;
}

void config_control_set_dev_mode(bool enable) {
    if (s_dev_mode_active == enable) return;

    s_dev_mode_active = enable;
    if (s_dev_mode_active) {
        current_system_state = STATE_DEV_MODE;
        ESP_LOGW(TAG, ">>> DEV MODE ACTIVATED! G29 LED and button UI engaged <<<");
        g29_led_ui_reset_throttle_cache();
        g29_led_ui_intro_animation();
        s_current_menu = CFG_MENU_ST_EPA;
        s_value_display_until = 0;
    } else {
        current_system_state = STATE_SYS_ACTIVE;
        ESP_LOGW(TAG, ">>> DEV MODE DEACTIVATED! Normal driving active <<<");
        
        // Exited Dev Mode: If config was modified, persist to NVS and send save command to C3
        if (s_config_dirty) {
            config_control_save_to_nvs();
            send_save_cmd_to_car();
            s_config_dirty = false;
        }

        g29_led_ui_exit_animation();
        g29_led_ui_clear();
        g29_led_ui_reset_throttle_cache();
    }
}

const car_config_packet_t* config_control_get_active_config(void) {
    return &g_active_config;
}

// Maps EPA value to 1-5 bar level
static uint8_t epa_to_bar_level(uint8_t epa) {
    if (epa <= 40) return 1;
    if (epa <= 55) return 2;
    if (epa <= 70) return 3;
    if (epa <= 85) return 4;
    return 5;
}

// Display menu value as LED bar
static void update_led_display(const g29_telemetry_t *telemetry) {
    TickType_t now = xTaskGetTickCount();

    if (now < s_value_display_until) {
        switch (s_current_menu) {
            case CFG_MENU_ST_EPA: {
                uint8_t epa_val;
                if (telemetry->steering < -0.2f) {
                    epa_val = g_active_config.st_reverse ? g_active_config.st_epa_right : g_active_config.st_epa_left;
                } else if (telemetry->steering > 0.2f) {
                    epa_val = g_active_config.st_reverse ? g_active_config.st_epa_left : g_active_config.st_epa_right;
                } else {
                    epa_val = g_active_config.st_epa_left;
                }
                g29_led_ui_show_bar(epa_to_bar_level(epa_val));
                break;
            }
            case CFG_MENU_ST_CURVE:
                // Curve 0: 1 LED, Curve 1: 2 LEDs, Curve 2: 3 LEDs
                g29_led_ui_show_bar(g_active_config.st_curve + 1);
                break;

            case CFG_MENU_TH_EPA: {
                uint8_t epa_val;
                if (telemetry->brake > 0.2f) {
                    epa_val = g_active_config.th_reverse ? g_active_config.th_epa_forward : g_active_config.th_epa_backward;
                } else {
                    epa_val = g_active_config.th_reverse ? g_active_config.th_epa_backward : g_active_config.th_epa_forward;
                }
                g29_led_ui_show_bar(epa_to_bar_level(epa_val));
                break;
            }
            case CFG_MENU_TH_CURVE:
                g29_led_ui_show_bar(g_active_config.th_curve + 1);
                break;

            case CFG_MENU_ST_TRIM: {
                int8_t trim = g_active_config.st_sub_trim;
                int8_t pos = 0;
                if (trim <= -20) pos = -2;
                else if (trim < -5) pos = -1;
                else if (trim <= 5) pos = 0;
                else if (trim < 20) pos = 1;
                else pos = 2;
                g29_led_ui_show_trim_position(pos);
                break;
            }
            case CFG_MENU_GYRO_GAIN: {
                // Gyro gain 0% - 100% displayed as 1-5 bars
                uint8_t gain = g_active_config.st_gyro_gain;
                uint8_t level = 1;
                if (gain <= 20) level = 1;
                else if (gain <= 40) level = 2;
                else if (gain <= 60) level = 3;
                else if (gain <= 80) level = 4;
                else level = 5;
                g29_led_ui_show_bar(level);
                break;
            }
            case CFG_MENU_CAR_SELECT: {
                // Display selected vehicle index on LEDs (Vehicle 1: LED 1, Vehicle 2: LED 2 ...)
                g29_led_ui_set_raw(1 << s_active_car_id);
                break;
            }
            default:
                break;
        }
    } else {
        // --- MENU SELECTION MODE (Active Menu LED blinks) ---
        bool blink_state = ((now / pdMS_TO_TICKS(250)) % 2) == 0;
        g29_led_ui_show_menu_single((uint8_t)s_current_menu, blink_state);
    }
}

bool config_control_process(const g29_telemetry_t *telemetry, car_config_packet_t *out_config_packet) {
    if (!telemetry) return false;

    uint32_t current_buttons = telemetry->buttons_state;
    uint32_t pressed = current_buttons & ~s_last_buttons; // Rising Edge
    s_last_buttons = current_buttons;

    bool config_changed = false;

    // ==============================================================
    // 1. DEV MODE TOGGLE COMBO (Hold for 1.5 seconds)
    //    OPTIONS + SHARE together OR PS button alone
    // ==============================================================
    bool combo_held = ((current_buttons & (BTN_SHARE | BTN_OPTIONS)) == (BTN_SHARE | BTN_OPTIONS)) ||
                      ((current_buttons & BTN_PS) != 0);

    if (combo_held) {
        if (!s_combo_latched) {
            s_combo_hold_ticks++;
            // 20ms * 75 = 1500ms (1.5 seconds)
            if (s_combo_hold_ticks >= 75) {
                config_control_set_dev_mode(!s_dev_mode_active);
                s_combo_latched = true; // Latch to prevent repeat triggering
                // Immediately send first packet upon entering Dev Mode
                if (s_dev_mode_active) {
                    config_changed = true;
                }
            }
        }
    } else {
        s_combo_hold_ticks = 0;
        s_combo_latched = false;
    }
    
    // When Dev Mode is inactive: Quick vehicle switch and live FFB toggle
    if (!s_dev_mode_active) {
        // LIVE FFB SIMULATION TOGGLE: When R3 button is pressed
        if (pressed & BTN_R3) {
            bool ffb_active = simulated_ffb_toggle();
            if (ffb_active) {
                ESP_LOGW(TAG, ">>> SIMULATED FFB ENABLED! <<<");
                g29_led_ui_set_raw(0x03); // 2 Green LEDs (ON feedback)
            } else {
                ESP_LOGW(TAG, ">>> SIMULATED FFB DISABLED (WHEEL FREE)! <<<");
                g29_led_ui_set_raw(0x10); // 1 Red LED (OFF feedback)
            }
            vTaskDelay(pdMS_TO_TICKS(180));
            g29_led_ui_clear();
            g29_led_ui_reset_throttle_cache();
        }

        // QUICK VEHICLE SWITCH: Hold ENTER (dial center) and turn Dial or press +/-
        if (current_buttons & BTN_ENTER) {
            bool next_btn = (pressed & (BTN_DIAL_RIGHT | BTN_PLUS  | BTN_DPAD_RIGHT | BTN_PADDLE_RIGHT)) != 0;
            bool prev_btn = (pressed & (BTN_DIAL_LEFT  | BTN_MINUS | BTN_DPAD_LEFT  | BTN_PADDLE_LEFT))  != 0;

            if (next_btn) {
                uint8_t next_id = (s_active_car_id + 1) % CAR_MAX_COUNT;
                config_control_select_car(next_id);
                // Show active vehicle LED
                g29_led_ui_set_raw(1 << s_active_car_id);
                vTaskDelay(pdMS_TO_TICKS(150));
                g29_led_ui_clear();
                g29_led_ui_reset_throttle_cache();
            } else if (prev_btn) {
                uint8_t prev_id = (s_active_car_id + CAR_MAX_COUNT - 1) % CAR_MAX_COUNT;
                config_control_select_car(prev_id);
                g29_led_ui_set_raw(1 << s_active_car_id);
                vTaskDelay(pdMS_TO_TICKS(150));
                g29_led_ui_clear();
                g29_led_ui_reset_throttle_cache();
            }
        }
        return false;
    }

    // ==============================================================
    // 2. MENU NAVIGATION (D-Pad Up / Down)
    // ==============================================================
    if (pressed & BTN_DPAD_UP) {
        s_current_menu = (cfg_menu_t)((s_current_menu + 1) % CFG_MENU_COUNT);
        s_value_display_until = 0; // Show menu indicator LED when switching menus
        ESP_LOGI(TAG, "Menu Selected -> [%d] (1:ST_EPA, 2:ST_CURVE, 3:TH_EPA, 4:TH_CURVE, 5:TRIM, 6:GYRO, 7:CAR_SEL)", s_current_menu + 1);
    } 
    else if (pressed & BTN_DPAD_DOWN) {
        s_current_menu = (cfg_menu_t)((s_current_menu + CFG_MENU_COUNT - 1) % CFG_MENU_COUNT);
        s_value_display_until = 0;
        ESP_LOGI(TAG, "Menu Selected -> [%d] (1:ST_EPA, 2:ST_CURVE, 3:TH_EPA, 4:TH_CURVE, 5:TRIM, 6:GYRO, 7:CAR_SEL)", s_current_menu + 1);
    }

    // ==============================================================
    // 3. VALUE ADJUSTMENT (+ / - Buttons, D-Pad Left / Right, Paddles)
    //    - + / - Buttons (Buttons above the red dial)
    //    - D-Pad Left / Right
    //    - Paddle shifters: Right (+), Left (-)
    // ==============================================================
    bool inc = (pressed & (BTN_PLUS  | BTN_DPAD_RIGHT | BTN_PADDLE_RIGHT)) != 0;
    bool dec = (pressed & (BTN_MINUS | BTN_DPAD_LEFT  | BTN_PADDLE_LEFT))  != 0;

    if (inc || dec) {
        s_value_display_until = xTaskGetTickCount() + pdMS_TO_TICKS(1800); // Show value bar for 1.8s
        config_changed = true;

        switch (s_current_menu) {
            
            // --- MENU 1: STEERING EPA ---
            case CFG_MENU_ST_EPA: {
                uint8_t *target_epa = NULL;
                const char *dir_name = NULL;

                if (telemetry->steering < -0.2f) {
                    // Wheel turned Left:
                    // If reversed, car steers right so Right EPA is adjusted, otherwise Left EPA
                    if (g_active_config.st_reverse) {
                        target_epa = &g_active_config.st_epa_right;
                        dir_name = "RIGHT (Reversed)";
                    } else {
                        target_epa = &g_active_config.st_epa_left;
                        dir_name = "LEFT";
                    }
                } 
                else if (telemetry->steering > 0.2f) {
                    // Wheel turned Right:
                    // If reversed, car steers left so Left EPA is adjusted, otherwise Right EPA
                    if (g_active_config.st_reverse) {
                        target_epa = &g_active_config.st_epa_left;
                        dir_name = "LEFT (Reversed)";
                    } else {
                        target_epa = &g_active_config.st_epa_right;
                        dir_name = "RIGHT";
                    }
                }

                if (target_epa != NULL) {
                    if (inc && *target_epa < 100) *target_epa += 5;
                    if (dec && *target_epa > 30)  *target_epa -= 5;
                    ESP_LOGI(TAG, "ST EPA [%s]: %d%% (Reverse: %s)", 
                             dir_name, *target_epa, g_active_config.st_reverse ? "ON" : "OFF");
                } 
                else {
                    // Wheel centered: Adjust both EPAs simultaneously (Dual Rate)
                    if (inc) {
                        if (g_active_config.st_epa_left < 100)  g_active_config.st_epa_left += 5;
                        if (g_active_config.st_epa_right < 100) g_active_config.st_epa_right += 5;
                    }
                    if (dec) {
                        if (g_active_config.st_epa_left > 30)  g_active_config.st_epa_left -= 5;
                        if (g_active_config.st_epa_right > 30) g_active_config.st_epa_right -= 5;
                    }
                    ESP_LOGI(TAG, "ST EPA [DUAL RATE]: Left=%d%% | Right=%d%%", 
                             g_active_config.st_epa_left, g_active_config.st_epa_right);
                }
                break;
            }

            // --- MENU 2: STEERING CURVE (EXPO) ---
            case CFG_MENU_ST_CURVE:
                if (inc && g_active_config.st_curve < 2) g_active_config.st_curve++;
                if (dec && g_active_config.st_curve > 0) g_active_config.st_curve--;
                ESP_LOGI(TAG, "ST CURVE: %s (%d)", 
                         g_active_config.st_curve == 0 ? "Linear" : 
                         (g_active_config.st_curve == 1 ? "Expo Soft" : "Expo Aggressive"), 
                         g_active_config.st_curve);
                break;

            // --- MENU 3: THROTTLE / BRAKE EPA ---
            case CFG_MENU_TH_EPA: {
                uint8_t *target_th_epa = NULL;
                const char *th_dir_name = NULL;

                if (telemetry->brake > 0.2f) {
                    // Brake pedal pressed:
                    if (g_active_config.th_reverse) {
                        target_th_epa = &g_active_config.th_epa_forward;
                        th_dir_name = "FORWARD (Reversed)";
                    } else {
                        target_th_epa = &g_active_config.th_epa_backward;
                        th_dir_name = "REVERSE/BRAKE";
                    }
                } else {
                    // Throttle pedal pressed or idle:
                    if (g_active_config.th_reverse) {
                        target_th_epa = &g_active_config.th_epa_backward;
                        th_dir_name = "REVERSE/BRAKE (Reversed)";
                    } else {
                        target_th_epa = &g_active_config.th_epa_forward;
                        th_dir_name = "FORWARD";
                    }
                }

                if (inc && *target_th_epa < 100) *target_th_epa += 5;
                if (dec && *target_th_epa > 20)  *target_th_epa -= 5;
                ESP_LOGI(TAG, "TH EPA [%s]: %d%% (Reverse: %s)", 
                         th_dir_name, *target_th_epa, g_active_config.th_reverse ? "ON" : "OFF");
                break;
            }

            // --- MENU 4: THROTTLE CURVE (EXPO) ---
            case CFG_MENU_TH_CURVE:
                if (inc && g_active_config.th_curve < 2) g_active_config.th_curve++;
                if (dec && g_active_config.th_curve > 0) g_active_config.th_curve--;
                ESP_LOGI(TAG, "TH CURVE: %s (%d)", 
                         g_active_config.th_curve == 0 ? "Linear" : 
                         (g_active_config.th_curve == 1 ? "Expo Soft" : "Expo Aggressive"), 
                         g_active_config.th_curve);
                break;

            // --- MENU 5: STEERING SUB-TRIM ---
            case CFG_MENU_ST_TRIM:
                if (inc && g_active_config.st_sub_trim < 50)  g_active_config.st_sub_trim += 5;
                if (dec && g_active_config.st_sub_trim > -50) g_active_config.st_sub_trim -= 5;
                ESP_LOGI(TAG, "ST SUB-TRIM: %d (PWM Offset: %d us)", 
                         g_active_config.st_sub_trim, g_active_config.st_sub_trim * 3);
                break;

            // --- MENU 6: GYRO GAIN (0% - 100%, GPIO 0 PWM) ---
            case CFG_MENU_GYRO_GAIN:
                if (inc && g_active_config.st_gyro_gain <= 95) g_active_config.st_gyro_gain += 5;
                if (dec && g_active_config.st_gyro_gain >= 5)  g_active_config.st_gyro_gain -= 5;
                ESP_LOGI(TAG, "GYRO GAIN: %d%% (PWM: %d us)", 
                         g_active_config.st_gyro_gain, 1000 + (g_active_config.st_gyro_gain * 10));
                break;

            // --- MENU 7: VEHICLE SELECTION (1 - 5) ---
            case CFG_MENU_CAR_SELECT: {
                if (inc) {
                    uint8_t next_id = (s_active_car_id + 1) % CAR_MAX_COUNT;
                    config_control_select_car(next_id);
                }
                if (dec) {
                    uint8_t prev_id = (s_active_car_id + CAR_MAX_COUNT - 1) % CAR_MAX_COUNT;
                    config_control_select_car(prev_id);
                }
                ESP_LOGI(TAG, "SELECTED VEHICLE: [Vehicle %d] (G29 LED %d)", s_active_car_id + 1, s_active_car_id + 1);
                break;
            }

            default:
                break;
        }
    }

    // ==============================================================
    // 4. CHANNEL REVERSE TOGGLE (TRIANGLE BUTTON)
    // ==============================================================
    if (pressed & BTN_TRIANGLE) {
        if (s_current_menu == CFG_MENU_ST_EPA || s_current_menu == CFG_MENU_ST_CURVE) {
            g_active_config.st_reverse = !g_active_config.st_reverse;
            ESP_LOGW(TAG, "ST REVERSE Toggled -> %s", g_active_config.st_reverse ? "REVERSED" : "NORMAL");
            config_changed = true;
        } else if (s_current_menu == CFG_MENU_TH_EPA || s_current_menu == CFG_MENU_TH_CURVE) {
            g_active_config.th_reverse = !g_active_config.th_reverse;
            ESP_LOGW(TAG, "TH REVERSE Toggled -> %s", g_active_config.th_reverse ? "REVERSED" : "NORMAL");
            config_changed = true;
        }
    }

    // ==============================================================
    // 5. CONFIRM / RESET (CROSS or ENTER Button)
    //    Single Click: Show Value | Hold 1.2s: Reset to Default
    // ==============================================================
    if (current_buttons & (BTN_ENTER | BTN_CROSS)) {
        if (!s_enter_latched) {
            s_enter_hold_ticks++;
            if (s_enter_hold_ticks >= 60) { // 1.2 seconds
                // Reset Active Menu to Default
                switch (s_current_menu) {
                    case CFG_MENU_ST_EPA:
                        g_active_config.st_epa_left = 100;
                        g_active_config.st_epa_right = 100;
                        break;
                    case CFG_MENU_ST_CURVE:
                        g_active_config.st_curve = 0;
                        break;
                    case CFG_MENU_TH_EPA:
                        g_active_config.th_epa_forward = 100;
                        g_active_config.th_epa_backward = 100;
                        break;
                    case CFG_MENU_TH_CURVE:
                        g_active_config.th_curve = 0;
                        break;
                    case CFG_MENU_ST_TRIM:
                        g_active_config.st_sub_trim = 0;
                        break;
                    case CFG_MENU_GYRO_GAIN:
                        g_active_config.st_gyro_gain = 50;
                        break;
                    case CFG_MENU_CAR_SELECT:
                        config_control_select_car(0); // Revert to first vehicle
                        break;
                    default:
                        break;
                }
                ESP_LOGW(TAG, "Selected Menu Reset to Defaults!");
                g29_led_ui_set_raw(G29_LED_ALL);
                vTaskDelay(pdMS_TO_TICKS(150));
                g29_led_ui_clear();
                s_value_display_until = xTaskGetTickCount() + pdMS_TO_TICKS(1500);
                config_changed = true;
                s_enter_latched = true;
            }
        }
    } else {
        if (s_enter_hold_ticks > 0 && !s_enter_latched) {
            // Short click release: Show value bar for 1.8s
            s_value_display_until = xTaskGetTickCount() + pdMS_TO_TICKS(1800);
        }
        s_enter_hold_ticks = 0;
        s_enter_latched = false;
    }

    // Update LED display
    update_led_display(telemetry);

    // If config changed, copy packet and mark dirty
    if (config_changed) {
        s_config_dirty = true;
        s_last_config_change_tick = xTaskGetTickCount();

        if (out_config_packet) {
            g_active_config.packet_type = PKT_TYPE_CONFIG;
            *out_config_packet = g_active_config;
        }
    }

    // Auto-save to NVS after 5s inactivity (safeguard against power loss before exiting Dev Mode)
    if (s_config_dirty && ((xTaskGetTickCount() - s_last_config_change_tick) > pdMS_TO_TICKS(5000))) {
        ESP_LOGI(TAG, "Auto-saving settings to NVS after 5s inactivity...");
        config_control_save_to_nvs();
        send_save_cmd_to_car();
        s_config_dirty = false;
    }

    return config_changed;
}

