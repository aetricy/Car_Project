#ifndef G29_CONFIG_H
#define G29_CONFIG_H

#include <stdint.h>


#define G29_RANGE                   900
#define G29_AUTOCENTER_STRENGHT     0
#define G29_AUTOCENTER_RATE         0

#define STEERING_DEADZONE           0.02f // %2'lik merkez ölü bölge (Direksiyon boşluğunu alır)
#define PEDAL_DEADZONE              0.05f      

#define LOGIWHEEL_BTN_PLUS         0x80 // 3.byte 1000 0000
#define LOGIWHEEL_BTN_MINUS        0x01 // 2.byte 

typedef struct {
    float steering;        // -1.0 (Tam Sol) ile 1.0 (Tam Sağ)
    float throttle;        // 0.0 (Bırakılmış) ile 1.0 (Tam Basılı)
    float brake;           // 0.0 (Bırakılmış) ile 1.0 (Tam Basılı)
    float clutch;          // 0.0 (Bırakılmış) ile 1.0 (Tam Basılı)

    bool button_plus;
    bool button_minus;
} g29_telemetry_t;

typedef struct {
    float battery_voltage; // Lipo durumu (G29 RPM LED'lerini yakmak için)
    uint8_t car_status;    // 0: OK, 1: Low Battery, 2: Error
} car_feedback_t;


#endif