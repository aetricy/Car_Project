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

#include "led_ui_on_s3.h"
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
// ESP-NOW GÖNDERİCİ TASK (Core 1) - G29 Aktif Olana Kadar Bekler
// -------------------------------------------------------------
void ESPNOW_Task(void *pvParameters) {
    car_drive_packet_t packet_to_send;
    
    ESP_LOGI(TAG, "ESPNOW_Task Beklemede...");

    // SİSTEM AKTİF OLANA KADAR RADROYU BAŞLATMA
    // Yani kullanıcı G29'u takıp system_state == STATE_SYS_ACTIVE olana kadar burada kilitli kalır.
    while (current_system_state != STATE_SYS_ACTIVE) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "G29 Aktif Oldu! ESP-NOW Kuruluyor...");
    init_esp_now_sender();

    while(1) {
        if (xQueueReceive(espnow_tx_queue, &packet_to_send, portMAX_DELAY) == pdTRUE) {
            send_telemetry_to_car(&packet_to_send);
        }
    }
}

// -------------------------------------------------------------
// 3. ANA DAĞITICI TASK (Logic Task - Core 1)
// -------------------------------------------------------------
void Logic_Task(void *pvParameters) {
    g29_telemetry_t incoming_telemetry;
    current_system_state = STATE_USB_WAITING; 
    
    // Uykuya geçişte veya hata durumunda tek seferlik komut yollamak için bayrak
    bool sleep_command_sent = false;

    while(1) {
        switch (current_system_state) {
            
            case STATE_USB_SETUP:
            case STATE_USB_WAITING:
            case STATE_USB_ENUMERATING:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case STATE_SYS_ACTIVE:
                sleep_command_sent = false; // Aktif moda dönünce bayrağı sıfırla

                if (xQueueReceive(g29_input_queue, &incoming_telemetry, pdMS_TO_TICKS(20)) == pdTRUE) {
                    
                    xTimerReset(sleep_timer, 0); 
                    
                    car_drive_packet_t drive_packet;

                    g29_create_drive_packet(&incoming_telemetry, &drive_packet);

                    drive_packet.packet_id   = global_packet_counter++;

                    xQueueSend(espnow_tx_queue, &drive_packet, 0);

                    if(xSemaphoreTake(ui_data_mutex, 0) == pdTRUE) { 
                        ui_shared_telemetry = incoming_telemetry;
                        xSemaphoreGive(ui_data_mutex);
                    }
                    
                    if (LOG_WHEELSTATE){
                        ESP_LOGI("TELEMETRY", "Str: %5.2f (->%d) | Thr: %4.2f (->%u) Packet ID: %d",  
                            incoming_telemetry.steering, drive_packet.steering,
                            incoming_telemetry.throttle, drive_packet.throttle,
                            drive_packet.packet_id
                        );
                    }
                }
                break;

            case STATE_SLEEP:
                // 1. Uykuya ilk kez girildiyse arabaya "UYKUYA GEÇ" komutu fırlat
                if (!sleep_command_sent) {
                    car_command_packet_t cmd_packet = {
                        .packet_type = PKT_TYPE_COMMAND,
                        .command_id  = CMD_SLEEP_ENTER,
                        .parameter   = 0
                    };
                    // Arabaya acil komut paketini gönder
                    xQueueSend(espnow_tx_queue, &cmd_packet, 0);
                    sleep_command_sent = true;
                    ESP_LOGW(TAG, "Araca UYKU Komutu (CMD_SLEEP_ENTER) Gönderildi.");
                }

                // Direksiyondan girdi gelirse uyan
                if (xQueueReceive(g29_input_queue, &incoming_telemetry, pdMS_TO_TICKS(100)) == pdTRUE) {
                    ESP_LOGI(TAG, "Araca UYANMA Komutu (CMD_WAKE_UP) Gönderildi.");
                    
                    // Arabaya "UYAN" komutu fırlat
                    car_command_packet_t wake_packet = {
                        .packet_type = PKT_TYPE_COMMAND,
                        .command_id  = CMD_WAKE_UP,
                        .parameter   = 0
                    };
                    xQueueSend(espnow_tx_queue, &wake_packet, 0);

                    current_system_state = STATE_SYS_ACTIVE;
                }
                break;

            case STATE_USB_DISCONNECTED:
                ESP_LOGE(TAG, "USB BAĞLANTISI KOPTU! Acil Failsafe Tetikleniyor...");
                
                // USB koptuğu an arabaya FAILSAFE (Acil Durdurma) komutu fırlat
                car_command_packet_t failsafe_packet = {
                    .packet_type = PKT_TYPE_COMMAND,
                    .command_id  = CMD_FAILSAFE_STOP,
                    .parameter   = 0
                };
                xQueueSend(espnow_tx_queue, &failsafe_packet, 0);

                xQueueReset(g29_input_queue);
                current_system_state = STATE_USB_WAITING;
                break;
                
            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Sistem Başlatılıyor...");

    System_Events = xEventGroupCreate();
    g29_input_queue = xQueueCreate(10, sizeof(g29_telemetry_t));
    ui_data_mutex = xSemaphoreCreateMutex();
    car_feedback_mutex = xSemaphoreCreateMutex();

    // Kuyruk boyutunu komut paketlerini de barındırabilecek boyutta ayarlıyoruz
    // (car_command_packet_t ve car_drive_packet_t union veya ortak boyut kullanabilir, 
    // en büyük struct olan car_drive_packet_t baz alınır)
    espnow_tx_queue = xQueueCreate(10, sizeof(car_drive_packet_t)); 
    
    sleep_timer = xTimerCreate("Sleep_Timer", pdMS_TO_TICKS(INACTIVITY_TIMEOUT_US / 1000), pdFALSE, (void *)0, sleep_timer_callback);

    if(System_Events == NULL || g29_input_queue == NULL || espnow_tx_queue == NULL || sleep_timer == NULL) {
        ESP_LOGE(TAG, "Kritik Hata: RTOS Objeleri Yaratılamadı!");
        return;
    }

    xTaskCreatePinnedToCore(Logic_Task, "Logic_Task", 8192, NULL, 4, NULL, OTHER_TASK_CORE);
    xTaskCreatePinnedToCore(ESPNOW_Task, "ESPNOW_Task", 4096, NULL, 3, NULL, OTHER_TASK_CORE);

    extern void LED_UI_Task(void *pvParameters);
    xTaskCreatePinnedToCore(LED_UI_Task, "LED_Task", 2048, NULL, 2, NULL, OTHER_TASK_CORE);

    if (g29_init(on_g29_state_changed, on_g29_input_received) == ESP_OK) {
        ESP_LOGI(TAG, "USB Sürücüsü Başarıyla Kuruldu. USB Bekleniyor...");
    } else {
        ESP_LOGE(TAG, "USB Sürücüsü Başlatılamadı!");
    }
}