#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdint.h"

#include "g29_config.h"
#include "esp_now_receive.h" // Modüler kütüphanemiz eklendi
#include "pwm_control.h"

#define LOG_MODE 1

void app_main(void) {
    // 1. Seri portun kararlı hale gelmesi için kısa bekleme
    vTaskDelay(pdMS_TO_TICKS(1000));

    init_default_config();
    init_pwm();
    
    // 2. ESP-NOW Alıcı Sistemini Kur
    init_esp_now_receiver();
    
    // Main içinde kullanacağımız veri paketi
    car_drive_packet_t current_telemetry;

    // 3. Ana Döngü
    while (1) {
        
        if (esp_now_get_latest_data(&current_telemetry)) {
            
            uint16_t steering_pwm = 1500;
            uint16_t throttle_pwm = 1500;
            // Logu burada, ana döngünün rahatlığında bas
            


            steering_pwm = apply_config_to_pwm(current_telemetry.steering, true);
            throttle_pwm = apply_config_to_pwm(current_telemetry.throttle, false);
            printf("TELEMETRY -> Steering: %d | Throttle: %d , iid : %d\n", 
                     steering_pwm, 
                     throttle_pwm,
                     current_telemetry.packet_id
                     );

                     
            set_steering_us(steering_pwm);
            set_throttle_us(throttle_pwm);

            }

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}