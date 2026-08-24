#include "g29_processor.h"
#include <stdio.h>

// Direksiyon verisini -1.0 ile 1.0 arasına haritalar
static float map_steering(uint16_t raw_val) {
    // 16-bit çözünürlük: Merkez 32768
    float val = ((float)raw_val - 32768.0f) / 32768.0f;
    
    // Donanımsal sınırlandırma (Clamp)
    if (val > 1.0f) val = 1.0f;
    if (val < -1.0f) val = -1.0f;

    // 1. Adım: Değer ölü bölgenin içindeyse tam 0.0 (Merkez) gönder
    if (val > -STEERING_DEADZONE && val < STEERING_DEADZONE) {
        return 0.0f;
    }

    // 2. Adım: Değer ölü bölgeden çıktığında atlama (jump) yapmasını engelle.
    // 0.0'dan pürüzsüzce başlayıp 1.0'a veya -1.0'a doğru lineer olarak ölçeklendir.
    if (val > 0.0f) {
        return (val - STEERING_DEADZONE) / (1.0f - STEERING_DEADZONE);
    } else {
        return (val + STEERING_DEADZONE) / (1.0f - STEERING_DEADZONE);
    }
}

// Pedal verilerini 0.0 ile 1.0 arasına haritalar ve ölü bölge uygular
static float map_pedal(uint8_t raw_val) {
    // Pedallar ters çalışır: 255 = Basılmamış, 0 = Tam Basılı
    float val = (255.0f - (float)raw_val) / 255.0f;
    
    // Sınırlandırma (Clamp)
    if (val > 1.0f) val = 1.0f;
    if (val < 0.0f) val = 0.0f;

    // 1. Adım: Ölü bölge kontrolü
    if (val < PEDAL_DEADZONE) {
        return 0.0f;
    }

    // 2. Adım: Atlama yapmaması için yumuşak kalkış ölçeklendirmesi
    return (val - PEDAL_DEADZONE) / (1.0f - PEDAL_DEADZONE);
}

void g29_process_raw_data(const uint8_t *raw_data, int len, g29_telemetry_t *out_telemetry) {
    if (len < 8) return; 

    // 1. Direksiyon Açısı (4. ve 5. bayt)
    uint16_t raw_steering = raw_data[4] | ((raw_data[5] << 8)); 
    out_telemetry->steering = map_steering(raw_steering);

    // 2. Pedallar (6., 7. ve 8. baytlar)
    out_telemetry->throttle = map_pedal(raw_data[6]);
    out_telemetry->brake    = map_pedal(raw_data[7]);
    out_telemetry->clutch   = map_pedal(raw_data[8]);

    // 3. Örnek Butonlar (Bit maskeleme ile)
    out_telemetry->button_plus   = (raw_data[2] & LOGIWHEEL_BTN_PLUS) != 0;
    out_telemetry->button_minus = (raw_data[3] & LOGIWHEEL_BTN_MINUS) != 0;
}


// IN DEVELOPING...
void g29_apply_drift_assist(g29_telemetry_t *telemetry, float gyro_yaw_rate) {
    // Örnek: Jiroskoptan gelen savrulma değerine göre direksiyona otonom müdahale
    // telemetry->steering -= gyro_yaw_rate * 0.1f;
    
    // Asistan sonrası değerin limitleri aşmasını engelleme (Safety Clamp)
    if (telemetry->steering > 1.0f) telemetry->steering = 1.0f;
    if (telemetry->steering < -1.0f) telemetry->steering = -1.0f;
}