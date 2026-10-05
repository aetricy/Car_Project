#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "CONFIG.h"
#include "g29_driver_host.h"
#include "g29_processor.h"
#include "esp_now_sender.h"

#include "s3_status_led.h"

#include "led_ui_on_g29.h"

static const char *TAG = "MAIN_APP";

// ==========================================
// FREERTOS OBJELERİ VE STATE DURUMLARI
// ==========================================
volatile s3_logic_state_t current_system_state = STATE_USB_SETUP;

EventGroupHandle_t System_Events;
#define EVT_G29_CONNECTED    (1 << 0)
#define EVT_SLEEP_ACTIVE     (1 << 2)

SemaphoreHandle_t ui_data_mutex;
g29_telemetry_t   ui_shared_telemetry;

SemaphoreHandle_t car_feedback_mutex;

QueueHandle_t g29_input_queue;
QueueHandle_t espnow_tx_queue; 
TimerHandle_t sleep_timer;

const int64_t INACTIVITY_TIMEOUT_US = INACTIVITY_TIMEOUT_us; 

static uint8_t global_packet_counter = 0;

// ==========================================
// YARDIMCI FONKSİYONLAR
// ==========================================

void sleep_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "30 Saniye Hareketsizlik! Uyku moduna geçiliyor...");
    current_system_state = STATE_SLEEP; 
}

// -------------------------------------------------------------
// 1. G29 GİRDİ (INPUT) CALLBACK
// -------------------------------------------------------------
void on_g29_input_received(const uint8_t *data, int len) {
    if(g29_is_ready() && (current_system_state == STATE_SYS_ACTIVE || current_system_state == STATE_SLEEP)){
        
        g29_telemetry_t temp_telemetry;
        g29_process_raw_data(data, len, &temp_telemetry);
         
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(g29_input_queue, &temp_telemetry, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

// -------------------------------------------------------------
// 2. G29 DURUM (STATE) CALLBACK 
// -------------------------------------------------------------
void on_g29_state_changed(g29_state_t state) {
    switch (state) {
        case G29_STATE_DISCONNECTED:
            if (current_system_state == STATE_SYS_ACTIVE) {
                current_system_state = STATE_USB_DISCONNECTED;
            }
            break;
            
        case G29_STATE_PS3_WAKING_UP:
            current_system_state = STATE_USB_ENUMERATING;
            break;
            
        case G29_STATE_NATIVE_READY:
            g29_disable_autocenter();
            g29_set_range(540);
            current_system_state = STATE_SYS_ACTIVE;
            break;
    }
}


// -------------------------------------------------------------
// 3. ANA DAĞITICI TASK (Logic Task - Core 1)
// -------------------------------------------------------------
void Logic_Task(void *pvParameters) {
    g29_telemetry_t incoming_telemetry;
    static g29_telemetry_t last_telemetry = {0};
    
    current_system_state = STATE_USB_WAITING; 
    bool sleep_command_sent = false;
    bool esp_now_started = false; // ESP-NOW başlatıldı mı kontrolü
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
                // SİSTEM İLK AKTİF OLDUĞUNDA ESP-NOW'U BAŞLAT
                if (!esp_now_started) {
                    ESP_LOGI(TAG, "G29 Aktif Oldu! ESP-NOW Kuruluyor...");
                    init_esp_now_sender();
                    esp_now_started = true;
                }

                if (car_needs_wakeup) {
                    espnow_tx_item_t wake_item;
                    memset(&wake_item, 0, sizeof(espnow_tx_item_t));
                    wake_item.length = sizeof(car_command_packet_t);
                    wake_item.payload.command.packet_type = PKT_TYPE_COMMAND;
                    wake_item.payload.command.command_id  = CMD_WAKE_UP;
                    wake_item.payload.command.parameter   = 0;

                    xQueueSend(espnow_tx_queue, &wake_item, 0);
                    car_needs_wakeup = false; // Uyandırdık, bayrağı indir
                    ESP_LOGI(TAG, "Araca WAKE_UP (Failsafe/Uyku Cikisi) Komutu Gonderildi.");
                }


                sleep_command_sent = false;
                bool new_data = false;

                while (xQueueReceive(g29_input_queue, &incoming_telemetry, 0) == pdTRUE) {
                    last_telemetry = incoming_telemetry;
                    new_data = true;
                }

                if (new_data) {
                    xTimerReset(sleep_timer, 0); 
                    if(xSemaphoreTake(ui_data_mutex, 0) == pdTRUE) { 
                        ui_shared_telemetry = last_telemetry;
                        xSemaphoreGive(ui_data_mutex);
                    }
                }
                
                // --- 2. SÜREKLİ SÜRÜŞ PAKETİ GÖNDERME ---
                espnow_tx_item_t tx_item;
                memset(&tx_item, 0, sizeof(espnow_tx_item_t)); // Çöpleri temizle (0x00)
                tx_item.length = sizeof(car_drive_packet_t);
                
                // Telemetriyi union içindeki drive paketine oluştur
                g29_create_drive_packet(&last_telemetry, &tx_item.payload.drive);
                tx_item.payload.drive.packet_type = PKT_TYPE_DRIVE; 
                tx_item.payload.drive.packet_id = global_packet_counter++;

                if (xQueueSend(espnow_tx_queue, &tx_item, 0) != pdTRUE) {
                    // SIRA DOLU hatası
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
                    ESP_LOGW(TAG, "Araca UYKU Komutu Gonderildi.");
                }

                if (xQueueReceive(g29_input_queue, &incoming_telemetry, pdMS_TO_TICKS(100)) == pdTRUE) {
                    ESP_LOGI(TAG, "Araca UYANMA Komutu Gonderildi.");
                    
                    espnow_tx_item_t wake_item;
                    memset(&wake_item, 0, sizeof(espnow_tx_item_t));
                    wake_item.length = sizeof(car_command_packet_t);
                    wake_item.payload.command.packet_type = PKT_TYPE_COMMAND;
                    wake_item.payload.command.command_id  = CMD_WAKE_UP;
                    wake_item.payload.command.parameter   = 0;

                    xQueueSend(espnow_tx_queue, &wake_item, 0);
                    current_system_state = STATE_SYS_ACTIVE;
                }
                break;

            case STATE_USB_DISCONNECTED:
                ESP_LOGE(TAG, "USB KOPTU! Acil Failsafe Tetikleniyor...");
                
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
    ESP_LOGI(TAG, "Sistem Baslatiliyor...");
    init_s3_status_led();

    System_Events = xEventGroupCreate();
    g29_input_queue = xQueueCreate(10, sizeof(g29_telemetry_t));
    ui_data_mutex = xSemaphoreCreateMutex();
    car_feedback_mutex = xSemaphoreCreateMutex();

    // Kuyruk artık UNION tipini (Kapsayıcıyı) taşıyor
    espnow_tx_queue = xQueueCreate(10, sizeof(espnow_tx_item_t)); 
    
    sleep_timer = xTimerCreate("Sleep_Timer", pdMS_TO_TICKS(INACTIVITY_TIMEOUT_US / 1000), pdFALSE, (void *)0, sleep_timer_callback);

    // xTaskCreatePinnedToCore(ESPNOW_Task... ) silindi, çünkü artık direkt sender task okuyor.
    xTaskCreatePinnedToCore(Logic_Task, "Logic_Task", 8192, NULL, 4, NULL, OTHER_TASK_CORE);
    

    if (g29_init(on_g29_state_changed, on_g29_input_received) == ESP_OK) {
        ESP_LOGI(TAG, "USB Surucusu Basariyla Kuruldu. USB Bekleniyor...");
    }
}