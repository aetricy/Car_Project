#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "g29_config.h"
#include "esp_now_receive.h" // Modüler kütüphanemiz eklendi

#define LOG_MODE 1

void app_main(void) {
    // 1. Seri portun kararlı hale gelmesi için kısa bekleme
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 2. ESP-NOW Alıcı Sistemini Kur
    init_esp_now_receiver();

    // Main içinde kullanacağımız veri paketi
    g29_telemetry_t current_telemetry;

    // 3. Ana Döngü
    while (LOG_MODE) {
        
        if (esp_now_get_latest_data(&current_telemetry)) {
            
            // Logu burada, ana döngünün rahatlığında bas
            printf("TELEMETRY -> Steering: %5.2f | Throttle: %4.2f | Brake: %4.2f\n", 
                     current_telemetry.steering, 
                     current_telemetry.throttle, 
                     current_telemetry.brake);
            }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}