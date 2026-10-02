#include "pwm_control.h"
#include "driver/ledc.h"
#include <stdio.h>

void init_pwm(void) {
    // 1. Timer Yapılandırması (330 Hz, 14-bit çözünürlük)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE, 
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_14_BIT,
        .freq_hz          = 330,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Direksiyon (Steering) Kanalı Yapılandırması
    ledc_channel_config_t ledc_channel_steering = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = STEERING_PWM_PIN,
        .duty           = 8110, // 330Hz'de 1500us'ye (merkez) denk gelen 14-bit duty
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_steering));

    // 3. Gaz/Fren (Throttle) Kanalı Yapılandırması
    ledc_channel_config_t ledc_channel_throttle = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = THROTTLE_PWM_PIN,
        .duty           = 8110, // 330Hz'de 1500us'ye (merkez/rölanti) denk gelen 14-bit duty
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_throttle));

    printf("-> [DONANIM] PWM pinleri basariyla baslatildi. Steering: GPIO%d, Throttle: GPIO%d\n", STEERING_PWM_PIN, THROTTLE_PWM_PIN);
}

// Steering değerini (1000 - 2000 mikrosaniye) PWM'e çevir
void set_steering_us(uint16_t raw_us) {
    // 1. Sınırları koru (Servo mekanizmasına zarar gelmemesi için)
    if (raw_us < 1000) raw_us = 1000;
    if (raw_us > 2000) raw_us = 2000;

    // 2. Doğrudan mikrosaniye -> 14-bit Duty (16384) dönüşümü (330Hz için)
    uint32_t duty = (uint32_t)(((float)raw_us * 16384.0f * 330.0f) / 1000000.0f);
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// Throttle değerini (1000 - 2000 mikrosaniye) PWM'e çevir
void set_throttle_us(uint16_t raw_us) {
    // 1. Sınırları koru
    if (raw_us < 1000) raw_us = 1000;
    if (raw_us > 2000) raw_us = 2000;

    // 2. Doğrudan mikrosaniye -> 14-bit Duty (16384) dönüşümü
    uint32_t duty = (uint32_t)(((float)raw_us * 16384.0f * 330.0f) / 1000000.0f);
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}