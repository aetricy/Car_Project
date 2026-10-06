#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "CONFIG.h"
#include "g29_driver_host.h"
#include "g29_processor.h"
#include "esp_now_sender.h"

#include "s3_status_led.h"
#include "led_ui_on_g29.h"
#include "config_control.h"
#include "simulated_ffb.h"

static const char *TAG = "MAIN_APP";

// ==========================================
// FREERTOS OBJECTS AND SYSTEM STATES
// ==========================================
volatile s3_logic_state_t current_system_state = STATE_USB_SETUP;

QueueHandle_t g29_input_queue;
QueueHandle_t espnow_tx_queue; 
TimerHandle_t sleep_timer;

const int64_t INACTIVITY_TIMEOUT_US = INACTIVITY_TIMEOUT_us; 

static uint8_t global_packet_counter = 0;

// ==========================================
// HELPER FUNCTIONS
// ==========================================

void sleep_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "30 seconds inactivity! Entering sleep mode...");
    current_system_state = STATE_SLEEP; 
}

// -------------------------------------------------------------
// 1. G29 INPUT CALLBACK
// -------------------------------------------------------------
void on_g29_input_received(const uint8_t *data, int len) {
    if (g29_is_ready() && (current_system_state == STATE_SYS_ACTIVE || current_system_state == STATE_DEV_MODE || current_system_state == STATE_SLEEP)) {
        g29_telemetry_t temp_telemetry;
        g29_process_raw_data(data, len, &temp_telemetry);
        xQueueSend(g29_input_queue, &temp_telemetry, 0);
    }
}

// -------------------------------------------------------------
// 2. G29 STATE CALLBACK 
// -------------------------------------------------------------
void on_g29_state_changed(g29_state_t state) {
    switch (state) {
        case G29_STATE_DISCONNECTED:
            if (current_system_state == STATE_SYS_ACTIVE || current_system_state == STATE_DEV_MODE) {
                current_system_state = STATE_USB_DISCONNECTED;
            }
            break;
            
        case G29_STATE_PS3_WAKING_UP:
            current_system_state = STATE_USB_ENUMERATING;
            break;
            
        case G29_STATE_NATIVE_READY:
            g29_set_range(540);
            simulated_ffb_init();
            current_system_state = STATE_SYS_ACTIVE;
            break;
    }
}


// -------------------------------------------------------------
// 3. MAIN DISPATCHER TASK (Logic Task - Core 1)
// -------------------------------------------------------------
void Logic_Task(void *pvParameters) {
    g29_telemetry_t incoming_telemetry;
    static g29_telemetry_t last_telemetry = {0};
    
    current_system_state = STATE_USB_WAITING; 
    bool sleep_command_sent = false;
    bool esp_now_started = false; // ESP-NOW initialization guard
    bool car_needs_wakeup = true;


    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    while(1) {
        switch (current_system_state) {
            
            case STATE_USB_SETUP:
            case STATE_USB_WAITING:
            case STATE_USB_ENUMERATING:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case STATE_SYS_ACTIVE:
            case STATE_DEV_MODE:
                // Initialize ESP-NOW on first active transition
                if (!esp_now_started) {
                    ESP_LOGI(TAG, "G29 became active! Initializing ESP-NOW...");
                    init_esp_now_sender();
                    esp_now_started = true;

                    // Send initial configuration packet to C3
                    espnow_tx_item_t init_cfg_item;
                    memset(&init_cfg_item, 0, sizeof(espnow_tx_item_t));
                    init_cfg_item.length = sizeof(car_config_packet_t);
                    init_cfg_item.payload.config = *config_control_get_active_config();
                    xQueueSend(espnow_tx_queue, &init_cfg_item, 0);
                    ESP_LOGI(TAG, "Initial Config packet sent to car.");
                }

                if (car_needs_wakeup) {
                    espnow_tx_item_t wake_item;
                    memset(&wake_item, 0, sizeof(espnow_tx_item_t));
                    wake_item.length = sizeof(car_command_packet_t);
                    wake_item.payload.command.packet_type = PKT_TYPE_COMMAND;
                    wake_item.payload.command.command_id  = CMD_WAKE_UP;
                    wake_item.payload.command.parameter   = 0;

                    xQueueSend(espnow_tx_queue, &wake_item, 0);
                    car_needs_wakeup = false; // Awakened, clear flag
                    ESP_LOGI(TAG, "Sent WAKE_UP command to car (Failsafe/Sleep exit).");
                }

                // Vehicle reconnected or powered on (Auto-Reconnect):
                if (esp_now_sender_check_and_clear_reconnected()) {
                    ESP_LOGI(TAG, ">>> CAR ONLINE! (Connection established) - Sending Config and Wake-up <<<");

                    // 1. Send wake-up command
                    espnow_tx_item_t wake_item;
                    memset(&wake_item, 0, sizeof(espnow_tx_item_t));
                    wake_item.length = sizeof(car_command_packet_t);
                    wake_item.payload.command.packet_type = PKT_TYPE_COMMAND;
                    wake_item.payload.command.command_id  = CMD_WAKE_UP;
                    wake_item.payload.command.parameter   = 0;
                    xQueueSend(espnow_tx_queue, &wake_item, 0);

                    // 2. Send current active configuration packet
                    espnow_tx_item_t cfg_item;
                    memset(&cfg_item, 0, sizeof(espnow_tx_item_t));
                    cfg_item.length = sizeof(car_config_packet_t);
                    cfg_item.payload.config = *config_control_get_active_config();
                    xQueueSend(espnow_tx_queue, &cfg_item, 0);
                }

                sleep_command_sent = false;
                bool new_data = false;

                while (xQueueReceive(g29_input_queue, &incoming_telemetry, 0) == pdTRUE) {
                    last_telemetry = incoming_telemetry;
                    new_data = true;
                }

                if (new_data) {
                    xTimerReset(sleep_timer, 0); 
                }

                // --- 1. DEV MODE & CONFIG PROCESSING (G29 BUTTONS & LED UI) ---
                car_config_packet_t updated_config;
                if (config_control_process(&last_telemetry, &updated_config)) {
                    espnow_tx_item_t cfg_item;
                    memset(&cfg_item, 0, sizeof(espnow_tx_item_t));
                    cfg_item.length = sizeof(car_config_packet_t);
                    cfg_item.payload.config = updated_config;
                    if (xQueueSend(espnow_tx_queue, &cfg_item, 0) == pdTRUE) {
                        ESP_LOGI(TAG, "Updated config packet enqueued to ESP-NOW (Size: %d)", cfg_item.length);
                    }
                }

                // --- 2. THROTTLE LED SYNCHRONIZATION IN DRIVING MODE ---
                if (current_system_state == STATE_SYS_ACTIVE) {
                    g29_led_ui_update_throttle(last_telemetry.throttle);
                }

                // --- 3. DYNAMIC SIMULATED FFB UPDATE ---
                simulated_ffb_update(&last_telemetry);

                // --- 4. CONTINUOUS DRIVE PACKET DISPATCH ---
                espnow_tx_item_t tx_item;
                memset(&tx_item, 0, sizeof(espnow_tx_item_t)); // Zero initialize
                tx_item.length = sizeof(car_drive_packet_t);
                
                // Populate drive packet from telemetry
                g29_create_drive_packet(&last_telemetry, &tx_item.payload.drive);
                tx_item.payload.drive.packet_type = PKT_TYPE_DRIVE; 
                tx_item.payload.drive.packet_id = global_packet_counter++;

                if (xQueueSend(espnow_tx_queue, &tx_item, 0) != pdTRUE) {
                    // Queue full dropped item
                }

                if (LOG_WHEELSTATE){
                    ESP_LOGI("TELEMETRY", "Str: %d | Thr: %d | ID: %u",  
                        tx_item.payload.drive.steering, 
                        tx_item.payload.drive.throttle, 
                        tx_item.payload.drive.packet_id);
                }

                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                break;

            case STATE_SLEEP:
                if (!sleep_command_sent) {
                    espnow_tx_item_t cmd_item;
                    memset(&cmd_item, 0, sizeof(espnow_tx_item_t));
                    cmd_item.length = sizeof(car_command_packet_t);
                    cmd_item.payload.command.packet_type = PKT_TYPE_COMMAND;
                    cmd_item.payload.command.command_id  = CMD_SLEEP_ENTER;
                    cmd_item.payload.command.parameter   = 0;

                    xQueueSend(espnow_tx_queue, &cmd_item, 0);
                    sleep_command_sent = true;
                    simulated_ffb_set_enabled(false); // Free motors (energy saving)
                    g29_led_ui_clear();
                    g29_led_ui_reset_throttle_cache();
                    ESP_LOGW(TAG, "Sent SLEEP command to car.");
                }

                if (xQueueReceive(g29_input_queue, &incoming_telemetry, pdMS_TO_TICKS(100)) == pdTRUE) {
                    ESP_LOGI(TAG, "Sent WAKE-UP command to car.");
                    
                    espnow_tx_item_t wake_item;
                    memset(&wake_item, 0, sizeof(espnow_tx_item_t));
                    wake_item.length = sizeof(car_command_packet_t);
                    wake_item.payload.command.packet_type = PKT_TYPE_COMMAND;
                    wake_item.payload.command.command_id  = CMD_WAKE_UP;
                    wake_item.payload.command.parameter   = 0;

                    xQueueSend(espnow_tx_queue, &wake_item, 0);
                    simulated_ffb_init(); // Re-initialize FFB with parked profile
                    g29_led_ui_reset_throttle_cache();
                    current_system_state = STATE_SYS_ACTIVE;
                }
                break;
            
            case STATE_USB_DISCONNECTED:
                ESP_LOGE(TAG, "USB DISCONNECTED! Triggering emergency failsafe...");
                simulated_ffb_reset();
                g29_led_ui_clear();
                g29_led_ui_reset_throttle_cache();
                
                espnow_tx_item_t fail_item;
                memset(&fail_item, 0, sizeof(espnow_tx_item_t));
                fail_item.length = sizeof(car_command_packet_t);
                fail_item.payload.command.packet_type = PKT_TYPE_COMMAND;
                fail_item.payload.command.command_id  = CMD_FAILSAFE_STOP;
                fail_item.payload.command.parameter   = 0;

                xQueueSend(espnow_tx_queue, &fail_item, 0);
                xQueueReset(g29_input_queue);

                xTimerStop(sleep_timer, 0);

                car_needs_wakeup = true;
                current_system_state = STATE_USB_WAITING;
                break;
                
            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}
void app_main(void) {
    ESP_LOGI(TAG, "System initializing...");
    init_s3_status_led();

    g29_input_queue = xQueueCreate(10, sizeof(g29_telemetry_t));
    espnow_tx_queue = xQueueCreate(10, sizeof(espnow_tx_item_t)); 
    
    sleep_timer = xTimerCreate("Sleep_Timer", pdMS_TO_TICKS(INACTIVITY_TIMEOUT_US / 1000), pdFALSE, (void *)0, sleep_timer_callback);

    config_control_init();

    // Logic_Task handles inputs and dispatches packets
    xTaskCreatePinnedToCore(Logic_Task, "Logic_Task", 8192, NULL, 4, NULL, OTHER_TASK_CORE);

    if (g29_init(on_g29_state_changed, on_g29_input_received)) {
        ESP_LOGI(TAG, "USB driver initialized successfully. Waiting for device...");
    } else {
        ESP_LOGE(TAG, "Failed to initialize USB host driver!");
    }
}