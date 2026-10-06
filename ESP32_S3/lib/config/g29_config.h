#ifndef G29_CONFIG_H
#define G29_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define G29_RANGE                   900
#define G29_AUTOCENTER_STRENGHT     0
#define G29_AUTOCENTER_RATE         0

#define STEERING_DEADZONE           0.02f // 2% center deadzone (eliminates center slop)
#define PEDAL_DEADZONE              0.05f      

#define LOGIWHEEL_BTN_PLUS          0x80  // Byte 3: 1000 0000
#define LOGIWHEEL_BTN_MINUS         0x01  // Byte 2 

// --- PACKET TYPES (OPCODES) ---
// Header flag specifying message type sent from S3 to C3
#define PKT_TYPE_DRIVE      0x01  // Real-time drive packet (1000-2000 PWM)
#define PKT_TYPE_COMMAND    0x02  // System commands (Sleep, Failsafe, Mode changes, etc.)
#define PKT_TYPE_CONFIG     0x03  // Configuration parameters written to NVS when changed

// --- SYSTEM COMMANDS (COMMAND ID) ---
#define CMD_SLEEP_ENTER     0x10  // S3 entered sleep, C3 enters sleep
#define CMD_WAKE_UP         0x11  // S3 woke up, C3 enters active mode
#define CMD_CONFIG_SAVE     0x12  // Dev Mode exited, commit settings to NVS immediately
#define CMD_FAILSAFE_STOP   0xEE  // Emergency Stop (Cable disconnected / S3 lost)

// ==========================================
// 1. S3 -> C3 REAL-TIME DRIVE PACKET (Streamed at 50Hz)
// ==========================================
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_DRIVE (0x01)
    uint8_t  packet_id;     
    
    // Transmitted in PWM format (1000-2000 us, Center: 1500)
    uint16_t steering;       // 1000 (Full Left) - 2000 (Full Right)
    uint16_t throttle;       // 1000 (Full Reverse/Brake) - 2000 (Full Forward)
} car_drive_packet_t;

// ==========================================
// 2. S3 -> C3 STATUS & COMMAND PACKET
// ==========================================
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_COMMAND (0x02)

    uint8_t  command_id;     // CMD_SLEEP_ENTER, CMD_FAILSAFE_STOP, etc.
    uint8_t  parameter;      // Optional data payload / mode ID
} car_command_packet_t;

// ==========================================
// 3. S3 -> C3 CONFIGURATION PACKET (Sent on changes/connect)
// ==========================================
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;     
    
    int8_t   st_gyro_gain;    // -100 to 100 

    int8_t   st_sub_trim;     // -100 to 100
    uint8_t  st_epa_left;     // 0-100%
    uint8_t  st_epa_right;    // 0-100%
    bool     st_reverse;      // true/false
    uint8_t  st_curve;        // 0: Linear, 1: Expo
    
    int8_t   th_sub_trim;     // -100 to 100
    uint8_t  th_epa_forward;  // 0-100%
    uint8_t  th_epa_backward; // 0-100%
    bool     th_reverse;      // true/false
    uint8_t  th_curve;        // 0: Linear, 1: Expo
} car_config_packet_t;

// ==========================================
// 4. FULL G29 TELEMETRY (Internal S3 only, not sent over ESP-NOW)
// ==========================================
typedef struct {
    float steering;          // -1.0 to 1.0
    float throttle;          // 0.0 to 1.0
    float brake;             // 0.0 to 1.0
    float clutch;            // 0.0 to 1.0
    uint32_t buttons_state;  // Button bitmask (Used locally on S3 for LED/UI)
} g29_telemetry_t;

// --- TRANSMITTER TX CONTAINER (UNION) ---
typedef struct {
    size_t length; // Actual transmission payload length
    union {
        car_drive_packet_t   drive;
        car_command_packet_t command;
        car_config_packet_t  config;
    } payload;
} espnow_tx_item_t;

// ==========================================
// 5. CAR RECEIVER SYSTEM STATES
// ==========================================
typedef enum {
    CAR_STATE_WAITING  = 1,
    CAR_STATE_ACTIVE   = 2,
    CAR_STATE_FAILSAFE = 3,
    CAR_STATE_SLEEP    = 4
} car_state_t;

#endif