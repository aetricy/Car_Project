#ifndef CONFIG_HOST_H
#define CONFIG_HOST_H

#define LOG_WHEELSTATE 0

#define USB_TASK_CORE 0
#define OTHER_TASK_CORE 1


#define ACTIVE_CAR_ID                       0 // Cihaz ID

static const uint8_t CAR_MAC_TABLE[][6] = {

    {0x24, 0x6F, 0x28, 0xAE, 0x32, 0xB0}, // Cihaz ID 0 (Örn: Drift Aracı 1 - Eray BMW)
    {0x24, 0x6F, 0x28, 0xAE, 0x32, 0xB1}, // Cihaz ID 1 (Örn: Drift Aracı 2 - Supra)
    {0x24, 0x6F, 0x28, 0xAE, 0x32, 0xB2}, // Cihaz ID 2 (Örn: Time Attack Aracı)
    {0x24, 0x6F, 0x28, 0xAE, 0x32, 0xB3}, // Cihaz ID 3 (Örn: Test Rig)
    {0x24, 0x6F, 0x28, 0xAE, 0x32, 0xB4}, // Cihaz ID 4 (Örn: Yedek Araç 1)
    {0x24, 0x6F, 0x28, 0xAE, 0x32, 0xB5}  // Cihaz ID 5 (Örn: Yedek Araç 2)


};

#endif // CONFIG_HOST_H