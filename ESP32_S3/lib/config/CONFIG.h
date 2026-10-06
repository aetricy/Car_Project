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

// ==========================================
// SİMÜLE EDİLMİŞ FORCE FEEDBACK (FFB) AYARLARI
// ==========================================
#define FFB_SIMULATION_ENABLED   1       // 1: Simüle FFB Açık, 0: Kapalı (Tamamen serbest/klasik direksiyon)
#define FFB_PARKED_FRICTION      0.28f   // Park halindeki hafif sertlik (0.0 - 1.0)
#define FFB_MIN_FRICTION         0.00f   // Sürüş halindeki sürtünme (0.00 = tüy gibi hafif, sıfır motor direnci)
#define FFB_MAX_FRICTION         0.25f   // Sert frende ulaşılabilecek maksimum sürtünme

#define FFB_PARKED_STRENGTH      0.08f   // Park halindeki merkezleme gücü (hafif)
#define FFB_MAX_STRENGTH         0.18f   // Gaza basınca / sürüşteki merkezleme gücü (yumuşak ve rahat)
#define FFB_PARKED_RATE          0.10f   // Park halindeki merkezleme eğimi
#define FFB_MAX_RATE             0.25f   // Gaza basınca / sürüşteki merkezleme eğimi (yumuşak geçiş)

#endif // CONFIG_HOST_H