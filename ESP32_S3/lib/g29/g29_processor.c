#include "g29_processor.h"
#include <stdio.h>
#include "esp_log.h"

// Maps raw steering data to [-1.0, 1.0] with deadzone
static float map_steering(uint16_t raw_val) {
    float val = ((float)raw_val - 32768.0f) / 32768.0f;
    
    if (val > 1.0f) val = 1.0f;
    if (val < -1.0f) val = -1.0f;

    if (val > -STEERING_DEADZONE && val < STEERING_DEADZONE) {
        return 0.0f;
    }

    if (val > 0.0f) {
        return (val - STEERING_DEADZONE) / (1.0f - STEERING_DEADZONE);
    } else {
        return (val + STEERING_DEADZONE) / (1.0f - STEERING_DEADZONE);
    }
}

// Maps raw pedal data to [0.0, 1.0] and applies deadzone
static float map_pedal(uint8_t raw_val) {
    float val = (255.0f - (float)raw_val) / 255.0f;
    
    if (val > 1.0f) val = 1.0f;
    if (val < 0.0f) val = 0.0f;

    if (val < PEDAL_DEADZONE) {
        return 0.0f;
    }

    return (val - PEDAL_DEADZONE) / (1.0f - PEDAL_DEADZONE);
}

void g29_process_raw_data(const uint8_t *raw_data, int len, g29_telemetry_t *out_telemetry) {
    if (len < 9) return; 

    // 1. Steering Angle (Bytes 4 and 5)
    uint16_t raw_steering = raw_data[4] | ((raw_data[5] << 8)); 
    out_telemetry->steering = map_steering(raw_steering);

    // 2. Pedals (Bytes 6, 7, and 8)
    out_telemetry->throttle = map_pedal(raw_data[6]);
    out_telemetry->brake    = map_pedal(raw_data[7]);
    out_telemetry->clutch   = map_pedal(raw_data[8]);

    // 3. Aggregate buttons into a single bitmask (buttons_state)
    out_telemetry->buttons_state = 0;

    // --- BYTE [0]: D-Pad and Shape Buttons ---
    // D-Pad: Byte 0 low nibble (values 0-7)
    uint8_t dpad_val = (raw_data[0]) & 0x0F;
    switch (dpad_val) {
        case 0: out_telemetry->buttons_state |= BTN_DPAD_UP; break;
        case 1: out_telemetry->buttons_state |= BTN_DPAD_UP | BTN_DPAD_RIGHT; break;
        case 2: out_telemetry->buttons_state |= BTN_DPAD_RIGHT; break;
        case 3: out_telemetry->buttons_state |= BTN_DPAD_RIGHT | BTN_DPAD_DOWN; break;
        case 4: out_telemetry->buttons_state |= BTN_DPAD_DOWN; break;
        case 5: out_telemetry->buttons_state |= BTN_DPAD_DOWN | BTN_DPAD_LEFT; break;
        case 6: out_telemetry->buttons_state |= BTN_DPAD_LEFT; break;
        case 7: out_telemetry->buttons_state |= BTN_DPAD_LEFT | BTN_DPAD_UP; break;
    }

    // Shapes: Byte 0 high nibble
    if (raw_data[0] & 0x10) out_telemetry->buttons_state |= BTN_CROSS;
    if (raw_data[0] & 0x20) out_telemetry->buttons_state |= BTN_SQUARE;
    if (raw_data[0] & 0x40) out_telemetry->buttons_state |= BTN_CIRCLE;
    if (raw_data[0] & 0x80) out_telemetry->buttons_state |= BTN_TRIANGLE;

    // --- BYTE [1]: L2/R2, Paddles, L3/R3, Share, and Options ---
    if (raw_data[1] & 0x10) out_telemetry->buttons_state |= BTN_SHARE;
    if (raw_data[1] & 0x20) out_telemetry->buttons_state |= BTN_OPTIONS;
    if (raw_data[1] & 0x40) out_telemetry->buttons_state |= BTN_R3;
    if (raw_data[1] & 0x80) out_telemetry->buttons_state |= BTN_L3;
    
    if (raw_data[1] & 0x01) out_telemetry->buttons_state |= BTN_PADDLE_RIGHT; // Shift Up
    if (raw_data[1] & 0x02) out_telemetry->buttons_state |= BTN_PADDLE_LEFT;  // Shift Down
    if (raw_data[1] & 0x04) out_telemetry->buttons_state |= BTN_R2;
    if (raw_data[1] & 0x08) out_telemetry->buttons_state |= BTN_L2;

    // --- BYTE [2]: PS Button and Plus Button ---
    if (raw_data[2] & 0x10) out_telemetry->buttons_state |= BTN_PS;
    if (raw_data[2] & 0x80) out_telemetry->buttons_state |= BTN_PLUS; // 1000 high nibble = 0x80

    // --- BYTE [3]: Rotary Dial and Minus Button ---
    if (raw_data[3] & 0x01) out_telemetry->buttons_state |= BTN_MINUS;
    if (raw_data[3] & 0x02) out_telemetry->buttons_state |= BTN_DIAL_RIGHT;
    if (raw_data[3] & 0x04) out_telemetry->buttons_state |= BTN_DIAL_LEFT;
    if (raw_data[3] & 0x08) out_telemetry->buttons_state |= BTN_ENTER;
}

// Stage 2: Converts float telemetry into 1000-2000 us PWM format for C3 receiver
void g29_create_drive_packet(const g29_telemetry_t *telemetry, car_drive_packet_t *out_packet) {
    
    out_packet->packet_type = PKT_TYPE_DRIVE;

    // 1. Map steering to 1000 - 2000 us range
    // telemetry->steering: -1.0 (Full Left) to +1.0 (Full Right)
    out_packet->steering = 1500 + (int16_t)(telemetry->steering * 500.0f);

    // 2. Combine pedals (RC ESC logic)
    // Neutral center at 1500us. Throttle extends to 2000us, brake to 1000us.
    uint16_t combined_throttle = 1500;
    
    if (telemetry->throttle > 0.0f) {
        combined_throttle = 1500 + (uint16_t)(telemetry->throttle * 500.0f);
    } 
    else if (telemetry->brake > 0.0f) {
        combined_throttle = 1500 - (uint16_t)(telemetry->brake * 500.0f);
    }

    out_packet->throttle = combined_throttle;
    
}