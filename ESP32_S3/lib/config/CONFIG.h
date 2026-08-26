#ifndef CONFIG_HOST_H
#define CONFIG_HOST_H

#define LOG_WHEELSTATE 0

#define USB_TASK_CORE 0
#define OTHER_TASK_CORE 1

#define INACTIVITY_TIMEOUT_us  30000000     // 30 saniye


#define ACTIVE_CAR_ID                       0 // Cihaz ID

static const uint8_t CAR_MAC_TABLE[][6] = {

    {0x90, 0x64, 0x9B, 0x08, 0x0E, 0x6C}, // Cihaz ID 0 (Örn: Drift Aracı 1 - Eray BMW)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Cihaz ID 1 (Örn: Drift Aracı 2 - Supra)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Cihaz ID 2 (Örn: Drift Aracı 2 - Supra)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Cihaz ID 3 (Örn: Drift Aracı 2 - Supra)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Cihaz ID 4 (Örn: Drift Aracı 2 - Supra)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Cihaz ID 5 (Örn: Drift Aracı 2 - Supra)

};

#endif // CONFIG_HOST_H