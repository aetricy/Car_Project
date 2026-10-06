#ifndef CONFIG_HOST_H
#define CONFIG_HOST_H

#define LOG_WHEELSTATE 0

#define USB_TASK_CORE 0
#define OTHER_TASK_CORE 1

#define INACTIVITY_TIMEOUT_us  30000000     // 30 saniye


#define CAR_MAX_COUNT                       5
#define ACTIVE_CAR_ID                       0 // Varsayılan Başlangıç Araç ID (0-4)

// S3 Logic State Tanımlamaları
typedef enum {
    STATE_USB_SETUP,
    STATE_USB_WAITING,
    STATE_USB_ENUMERATING,
    STATE_SYS_ACTIVE,
    STATE_USB_DISCONNECTED,
    STATE_SLEEP,
    STATE_DEV_MODE
} s3_logic_state_t;

// Araç MAC Tablosu (5 Araç Destekli - G29 LED 1-5)
static const uint8_t CAR_MAC_TABLE[CAR_MAX_COUNT][6] = {
    {0x90, 0x64, 0x9B, 0x08, 0x0E, 0x6C}, // Araç 1 (ID 0)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Araç 2 (ID 1)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Araç 3 (ID 2)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Araç 4 (ID 3)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Araç 5 (ID 4)
};

#endif // CONFIG_HOST_H