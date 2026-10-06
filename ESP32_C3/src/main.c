#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"

#include "g29_config.h"
#include "esp_now_receive.h"
#include "pwm_control.h"
#include "led_indicator.h"

#define LOG_MODE 1

#define STATE_WAITING  1
#define STATE_ACTIVE   2
#define STATE_FAILSAFE 3
#define STATE_SLEEP    4

// BAĞLANTI KOPMA SÜRESİ (Milisaniye)
// 500ms boyunca S3'ten paket gelmezse araç otomatik durur.
#define CONNECTION_TIMEOUT_MS 500 

volatile int current_state = STATE_WAITING; 

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    init_default_config();
    init_pwm();
    init_esp_now_receiver();
    
    init_led_indicator();
    
    car_drive_packet_t   current_telemetry;
    car_command_packet_t current_command;
    car_config_packet_t  current_config;

    // Son paket alınma zamanını tutacak değişken
    TickType_t last_packet_time = xTaskGetTickCount();

    while (1) {
        
        // ==========================================
        // A. KOMUT (COMMAND) KONTROLÜ
        // ==========================================
        if (esp_now_get_command_data(&current_command)) {
            last_packet_time = xTaskGetTickCount(); // Sinyal geldi, zamanlayıcıyı sıfırla

            switch (current_command.command_id) {
                case CMD_WAKE_UP:
                    current_state = STATE_ACTIVE;
                    printf("[SİSTEM] KOMUT ALINDI: UYAN! Arac Aktif.\n");
                    break;
                    
                case CMD_SLEEP_ENTER:
                    current_state = STATE_SLEEP;
                    printf("[SİSTEM] KOMUT ALINDI: UYKU MODU. Motorlar Durduruluyor.\n");
                    set_steering_us(1500);
                    set_throttle_us(1500);
                    break;
                    
                case CMD_FAILSAFE_STOP:
                    current_state = STATE_FAILSAFE;
                    printf("[SİSTEM] ACİL DURUM! USB Koptu, Arac Kilitlendi.\n");
                    set_steering_us(1500); 
                    set_throttle_us(1500); 
                    break;
            }
        }

        // ==========================================
        // B. AYAR (CONFIG) KONTROLÜ
        // ==========================================
        if (esp_now_get_config_data(&current_config)) {
            last_packet_time = xTaskGetTickCount(); // Sinyal geldi, zamanlayıcıyı sıfırla
            update_pwm_config(&current_config);     // Ayarları anında PWM motor/servo sistemine uygula
            printf("[SİSTEM] YENI AYARLAR ALINDI VE UYGULANDI!\n");
        }

        // ==========================================
        // C. SÜRÜŞ (DRIVE) VERİSİ KONTROLÜ
        // ==========================================
        bool drive_data_received = esp_now_get_latest_data(&current_telemetry);
        
        if (drive_data_received) {
            last_packet_time = xTaskGetTickCount(); // Sinyal geldi, zamanlayıcıyı sıfırla
            
            // Eğer araç menzil dışına çıkıp Failsafe'e düştüyse ve 
            // tekrar menzile girip paket almaya başladıysa OTOMATİK uyan.
            // (Not: USB kopsaydı S3 zaten drive paketi yollamayı keserdi, 
            // yani paket geliyorsa bağlantı tamamen sağlıklıdır.)
            if (current_state == STATE_FAILSAFE) {
                current_state = STATE_ACTIVE;
                printf("[SİSTEM] Baglanti Tekrar Kuruldu! Arac Otomatik Aktif.\n");
            }

            if (current_state == STATE_ACTIVE) {
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
        // D. BAĞLANTI KOPMA (WATCHDOG/TIMEOUT) KONTROLÜ
        // ==========================================
        // Sadece araç aktifken bağlantı kopmasını dert ederiz. Uyurken kopması önemli değil.
        if (current_state == STATE_ACTIVE) {
            TickType_t current_time = xTaskGetTickCount();
            uint32_t elapsed_time_ms = (current_time - last_packet_time) * portTICK_PERIOD_MS;

            if (elapsed_time_ms > CONNECTION_TIMEOUT_MS) {
                current_state = STATE_FAILSAFE; // Aracı kilitle
                set_steering_us(1500);          // Direksiyonu düzle
                set_throttle_us(1500);          // Gazı kes
                printf("\n[SİSTEM - HATA] %lu ms BOYUNCA SİNYAL ALINAMADI!\n", elapsed_time_ms);
                printf("[SİSTEM] MENZİL DIŞI VEYA S3 KAPANDI. OTOMATİK FAILSAFE AKTİF!\n\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}