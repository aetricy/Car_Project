#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "g29_config.h"
#include "esp_now_receive.h" // Modüler kütüphanemiz eklendi
#include "pwm_control.h"

#define LOG_MODE 0


// Başlangıç değerleri nötr/merkez olan 1500us olarak ayarlandı
volatile uint16_t current_steering_us = 2000;
volatile uint16_t current_throttle_us = 1300;




void app_main(void) {
    // 1. Seri portun kararlı hale gelmesi için kısa bekleme
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 2. ESP-NOW Alıcı Sistemini Kur
    init_esp_now_receiver();

    // Main içinde kullanacağımız veri paketi
    g29_telemetry_t current_telemetry;

    init_pwm();


    // 3. Ana Dongu
    while (1) {
        // Mikrosaniye değerlerini donanıma yaz
        set_steering_us(current_steering_us);
        set_throttle_us(current_throttle_us);

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}