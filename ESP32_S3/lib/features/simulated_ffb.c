#include "simulated_ffb.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "g29_driver_host.h"

static const char *TAG = "SIM_FFB";

static float s_estimated_speed = 0.0f;
static int64_t s_last_update_us = 0;
static int64_t s_last_send_us = 0;

static uint8_t s_current_s_val = 0xFF;
static uint8_t s_current_r_val = 0xFF;
static uint8_t s_current_f_val = 0xFF;
static bool s_enabled = FFB_SIMULATION_ENABLED;

void simulated_ffb_init(void) {
    if (!s_enabled) {
        if (g29_is_ready()) {
            g29_set_friction_raw(0);
            g29_disable_autocenter();
        }
        ESP_LOGI(TAG, "FFB Simulasyonu Kapali (Direksiyon Serbest Modda).");
        return;
    }

    if (!g29_is_ready()) return;

    g29_enable_autocenter();

    s_estimated_speed = 0.0f;
    s_last_update_us = esp_timer_get_time();
    s_last_send_us = s_last_update_us;

    // Park halindeki başlangıç değerleri
    uint8_t s_val = (uint8_t)(FFB_PARKED_STRENGTH * 15.0f + 0.5f);
    uint8_t r_val = (uint8_t)(FFB_PARKED_RATE * 255.0f + 0.5f);
    uint8_t f_val = (uint8_t)(FFB_PARKED_FRICTION * 7.0f + 0.5f);

    g29_set_autocenter_raw(s_val, r_val);
    g29_set_friction_raw(f_val);

    s_current_s_val = s_val;
    s_current_r_val = r_val;
    s_current_f_val = f_val;

    ESP_LOGI(TAG, "Simule FFB Baslatildi (Park Hali: Guc=%d/15, Egim=%d/255, Surtunme=%d/7)", s_val, r_val, f_val);
}

void simulated_ffb_set_enabled(bool enabled) {
    s_enabled = enabled;
    if (!g29_is_ready()) return;

    if (!enabled) {
        // Uyku modunda veya kapatıldığında motor akımını ve kuvvetlerini kes
        g29_set_friction_raw(0);
        g29_disable_autocenter();
        s_current_s_val = 0xFF;
        s_current_r_val = 0xFF;
        s_current_f_val = 0xFF;
        ESP_LOGI(TAG, "FFB Devre Disi Birakildi (Motorlar Serbest).");
    } else {
        simulated_ffb_init();
    }
}

bool simulated_ffb_is_enabled(void) {
    return s_enabled;
}

bool simulated_ffb_toggle(void) {
    simulated_ffb_set_enabled(!s_enabled);
    return s_enabled;
}

void simulated_ffb_reset(void) {
    s_estimated_speed = 0.0f;
    s_last_update_us = esp_timer_get_time();
    if (s_enabled && g29_is_ready()) {
        uint8_t s_val = (uint8_t)(FFB_PARKED_STRENGTH * 15.0f + 0.5f);
        uint8_t r_val = (uint8_t)(FFB_PARKED_RATE * 255.0f + 0.5f);
        uint8_t f_val = (uint8_t)(FFB_PARKED_FRICTION * 7.0f + 0.5f);

        g29_set_autocenter_raw(s_val, r_val);
        g29_set_friction_raw(f_val);

        s_current_s_val = s_val;
        s_current_r_val = r_val;
        s_current_f_val = f_val;
    } else if (g29_is_ready()) {
        g29_set_friction_raw(0);
        g29_disable_autocenter();
    }
}

float simulated_ffb_get_estimated_speed(void) {
    return s_estimated_speed;
}

void simulated_ffb_update(const g29_telemetry_t *telemetry) {
    if (!s_enabled || !g29_is_ready() || telemetry == NULL) return;

    int64_t now = esp_timer_get_time();
    if (s_last_update_us == 0) {
        s_last_update_us = now;
        return;
    }

    float dt = (float)(now - s_last_update_us) / 1000000.0f;
    s_last_update_us = now;

    if (dt <= 0.001f || dt > 0.2f) {
        dt = 0.02f; // Emniyet filtresi (20ms)
    }

    float throttle  = telemetry->throttle;
    float brake     = telemetry->brake;
    float steering  = telemetry->steering;
    float abs_steer = fabsf(steering);

    // =========================================================================
    // 1. TAHMİNİ ARAÇ HIZI SİMÜLASYONU
    // =========================================================================
    if (throttle > 0.05f) {
        // Hızlanma: Tam gazda yaklaşık 1.25 saniyede maksimum hıza yaklaşır
        float accel = 0.80f * throttle;
        float drag  = 0.30f * s_estimated_speed * s_estimated_speed;
        s_estimated_speed += (accel - drag) * dt;
    } else if (brake > 0.05f) {
        // Fren: Güçlü frenleme (yaklaşık 0.45 saniyede duruş)
        float decel = 2.20f * brake;
        s_estimated_speed -= decel * dt;
    } else {
        // Boş süzülme: Mekanik direnç ve yuvarlanma sürtünmesi aracı yavaşlatır
        float coast_drag = 0.45f * s_estimated_speed + 0.08f;
        s_estimated_speed -= coast_drag * dt;
    }

    // Hız sınırlandırma [0.0, 1.0]
    if (s_estimated_speed < 0.01f) {
        s_estimated_speed = 0.0f;
    } else if (s_estimated_speed > 1.0f) {
        s_estimated_speed = 1.0f;
    }

    // =========================================================================
    // 2. MEKANİK SÜRTÜNME (AĞIRLIK / DİRENÇ)
    //    - Park halinde (hız=0) direksiyon hafif oturaklıdır.
    //    - Gaza basıldığı anda ve araç hareket ettiğinde sürtünme sıfırlanır,
    //      direksiyon yumuşacık ve rahatça döner.
    // =========================================================================
    float drive_factor = s_estimated_speed;
    if (throttle > drive_factor) {
        drive_factor = throttle; // Gaza basıldığı anda beklemeden anında yumuşasın!
    }

    float target_friction = FFB_PARKED_FRICTION * (1.0f - drive_factor);

    // Frenleme anında ön aksa binen yük (ağırlık transferi) direksiyonu hafif sertleştirir
    if (brake > 0.1f) {
        target_friction += 0.08f * brake;
    }

    if (target_friction < FFB_MIN_FRICTION) target_friction = FFB_MIN_FRICTION;
    if (target_friction > FFB_MAX_FRICTION) target_friction = FFB_MAX_FRICTION;

    // =========================================================================
    // 3. MERKEZLEME GÜCÜ VE HIZI (CASTER YAY ETKİSİ / SELF-ALIGNING TORQUE)
    //    - Dönüşlerde ve hızlandıkça kaster açısı direksiyonu tatlı ve kolay bir şekilde merkeze çeker.
    // =========================================================================
    float target_strength = FFB_PARKED_STRENGTH + (FFB_MAX_STRENGTH - FFB_PARKED_STRENGTH) * s_estimated_speed;
    float target_rate     = FFB_PARKED_RATE + (FFB_MAX_RATE - FFB_PARKED_RATE) * s_estimated_speed;

    // Virajda önden kayma (Understeer) simülasyonu:
    // Yüksek hızda direksiyon çok fazla kırıldığında ön lastik tutunması azalır, merkezleme yay direnci hafifçe yumuşar
    if (s_estimated_speed > 0.50f && abs_steer > 0.75f) {
        float scrub_factor = (abs_steer - 0.75f) / 0.25f; // 0.0 - 1.0
        target_strength *= (1.0f - 0.20f * scrub_factor);
    }

    // Frenleme sırasında kaster tutunması ve direnç hafifçe belirginleşir
    if (brake > 0.1f) {
        target_strength += 0.05f * brake;
    }

    if (target_strength > 1.0f) target_strength = 1.0f;
    if (target_rate > 1.0f) target_rate = 1.0f;

    // =========================================================================
    // 4. DONANIMSAL KADEMELERE DÖNÜŞTÜRME VE NON-BLOCKING AKILLI USB GÖNDERİMİ
    // =========================================================================
    uint8_t s_val = (uint8_t)(target_strength * 15.0f + 0.5f);
    uint8_t r_val = (uint8_t)(target_rate * 255.0f + 0.5f);
    uint8_t f_val = (uint8_t)(target_friction * 7.0f + 0.5f);

    bool heartbeat = (now - s_last_send_us) > 500000; // 500ms heartbeat senkronizasyonu

    // Yalnızca donanım kademesi değiştiğinde veya 500ms geçtiğinde USB paketi gönder
    if (heartbeat || s_val != s_current_s_val || abs((int)r_val - (int)s_current_r_val) > 4) {
        g29_set_autocenter_raw(s_val, r_val);
        s_current_s_val = s_val;
        s_current_r_val = r_val;
        s_last_send_us = now;
    }

    if (heartbeat || f_val != s_current_f_val) {
        g29_set_friction_raw(f_val);
        s_current_f_val = f_val;
        s_last_send_us = now;
    }
}

