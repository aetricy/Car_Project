#ifndef CONFIG_HOST_H
#define CONFIG_HOST_H

#define LOG_WHEELSTATE 0

#define USB_TASK_CORE 0
#define OTHER_TASK_CORE 1

#define INACTIVITY_TIMEOUT_us  30000000     // 30 seconds

#define CAR_MAX_COUNT                       5
#define ACTIVE_CAR_ID                       0 // Default Initial Vehicle ID (0-4)

// S3 Logic State Definitions
typedef enum {
    STATE_USB_SETUP,
    STATE_USB_WAITING,
    STATE_USB_ENUMERATING,
    STATE_SYS_ACTIVE,
    STATE_USB_DISCONNECTED,
    STATE_SLEEP,
    STATE_DEV_MODE
} s3_logic_state_t;

// Vehicle MAC Table (5 Vehicles Supported - G29 LEDs 1-5)
static const uint8_t CAR_MAC_TABLE[CAR_MAX_COUNT][6] = {
    {0x90, 0x64, 0x9B, 0x08, 0x0E, 0x6C}, // Vehicle 1 (ID 0)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Vehicle 2 (ID 1)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Vehicle 3 (ID 2)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Vehicle 4 (ID 3)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Vehicle 5 (ID 4)
};

// ==========================================
// SIMULATED FORCE FEEDBACK (FFB) SETTINGS
// ==========================================
#define FFB_SIMULATION_ENABLED   1       // 1: Simulated FFB Enabled, 0: Disabled (Wheel runs completely free)
#define FFB_PARKED_FRICTION      0.28f   // Parked mechanical friction (0.0 - 1.0)
#define FFB_MIN_FRICTION         0.00f   // Driving friction (0.00 = feather-light, zero motor drag)
#define FFB_MAX_FRICTION         0.25f   // Maximum friction under heavy braking

#define FFB_PARKED_STRENGTH      0.08f   // Parked centering spring strength (light)
#define FFB_MAX_STRENGTH         0.18f   // Driving centering spring strength (smooth and gentle)
#define FFB_PARKED_RATE          0.10f   // Parked centering ramp slope
#define FFB_MAX_RATE             0.25f   // Driving centering ramp slope (smooth transition)

#endif // CONFIG_HOST_H