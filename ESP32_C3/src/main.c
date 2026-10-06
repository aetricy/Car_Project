#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"

#include "g29_config.h"
#include "esp_now_receive.h"
#include "pwm_control.h"
#include "led_indicator.h"

#define LOG_MODE 1

// CONNECTION TIMEOUT (Milliseconds)
// If no packet is received from S3 for 500ms, vehicle failsafes automatically.
#define CONNECTION_TIMEOUT_MS 500 

volatile car_state_t current_state = CAR_STATE_WAITING; 

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    init_esp_now_receiver();
    init_pwm();
    init_config_with_nvs(); // Load stored configuration from NVS (or assign defaults)
    
    init_led_indicator();
    
    car_drive_packet_t   current_telemetry;
    car_command_packet_t current_command;
    car_config_packet_t  current_config;

    // NVS Flash Wear Protection (Debounce and Dirty flag)
    bool config_dirty = false;
    TickType_t last_config_rx_time = 0;

    // Timestamp of last received packet
    TickType_t last_packet_time = xTaskGetTickCount();

    while (1) {
        
        // ==========================================
        // A. COMMAND PACKET PROCESSING
        // ==========================================
        if (esp_now_get_command_data(&current_command)) {
            last_packet_time = xTaskGetTickCount(); // Reset watchdog timer on packet arrival

            switch (current_command.command_id) {
                case CMD_CONFIG_SAVE:
                    printf("[SYSTEM] COMMAND RECEIVED: Dev Mode exited, saving settings to NVS...\n");
                    if (config_dirty) {
                        save_config_to_nvs(get_current_config());
                        config_dirty = false;
                    }
                    break;

                case CMD_WAKE_UP:
                    current_state = CAR_STATE_ACTIVE;
                    printf("[SYSTEM] COMMAND RECEIVED: WAKE UP! Vehicle Active.\n");
                    break;
                    
                case CMD_SLEEP_ENTER:
                    current_state = CAR_STATE_SLEEP;
                    printf("[SYSTEM] COMMAND RECEIVED: SLEEP MODE. Neutralizing actuators.\n");
                    set_steering_us(1500);
                    set_throttle_us(1500);
                    break;
                    
                case CMD_FAILSAFE_STOP:
                    current_state = CAR_STATE_FAILSAFE;
                    printf("[SYSTEM] EMERGENCY! USB Disconnected, Vehicle Locked.\n");
                    set_steering_us(1500); 
                    set_throttle_us(1500); 
                    break;
            }
        }

        // ==========================================
        // B. CONFIGURATION PACKET PROCESSING (Applies to RAM instantly, no flash latency)
        // ==========================================
        if (esp_now_get_config_data(&current_config)) {
            last_packet_time = xTaskGetTickCount(); // Reset watchdog timer on packet arrival
            update_pwm_config(&current_config);     // Apply parameters to PWM system instantly (zero latency)
            config_dirty = true;
            last_config_rx_time = xTaskGetTickCount();
            printf("[SYSTEM] NEW CONFIGURATION APPLIED TO RAM (Flash pending)!\n");
        }

        // Auto-save to NVS after 5s inactivity (safeguard against power-down during Dev Mode)
        if (config_dirty && ((xTaskGetTickCount() - last_config_rx_time) > pdMS_TO_TICKS(5000))) {
            printf("[NVS] Auto-saving settings to NVS after 5s inactivity...\n");
            save_config_to_nvs(get_current_config());
            config_dirty = false;
        }

        // ==========================================
        // C. DRIVE TELEMETRY PROCESSING
        // ==========================================
        bool drive_data_received = esp_now_get_latest_data(&current_telemetry);
        
        if (drive_data_received) {
            last_packet_time = xTaskGetTickCount(); // Reset watchdog timer on packet arrival
            
            // If vehicle is in CAR_STATE_WAITING or recovering from failsafe,
            // automatically switch to ACTIVE state upon receiving valid drive packets!
            if (current_state == CAR_STATE_WAITING || current_state == CAR_STATE_FAILSAFE) {
                current_state = CAR_STATE_ACTIVE;
                printf("[SYSTEM] Packet Received! Vehicle Automatically Switched to Active Mode.\n");
            }

            if (current_state == CAR_STATE_ACTIVE) {
                uint16_t steering_pwm = apply_config_to_pwm(current_telemetry.steering, true);
                uint16_t throttle_pwm = apply_config_to_pwm(current_telemetry.throttle, false);
                
                if (LOG_MODE) {
                    printf("TELEMETRY -> Steering: %d | Throttle: %d , pkt_id : %d\n", 
                             steering_pwm, throttle_pwm, current_telemetry.packet_id);
                }

                set_steering_us(steering_pwm);
                set_throttle_us(throttle_pwm);
            }
        }

        // ==========================================
        // D. CONNECTION TIMEOUT (WATCHDOG) MONITORING
        // ==========================================
        // Only monitor link loss during active driving state.
        if (current_state == CAR_STATE_ACTIVE) {
            TickType_t current_time = xTaskGetTickCount();
            uint32_t elapsed_time_ms = (current_time - last_packet_time) * portTICK_PERIOD_MS;

            if (elapsed_time_ms > CONNECTION_TIMEOUT_MS) {
                current_state = CAR_STATE_FAILSAFE; // Lock vehicle into failsafe
                set_steering_us(1500);          // Neutralize steering
                set_throttle_us(1500);          // Cut throttle
                printf("\n[SYSTEM - ERROR] NO SIGNAL FOR %lu ms!\n", elapsed_time_ms);
                printf("[SYSTEM] OUT OF RANGE OR S3 POWERED OFF. AUTOMATIC FAILSAFE ENGAGED!\n\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}