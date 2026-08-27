TODO: Bidirectional Telemetry

// C3 -> S3 Geri Bildirim (Telemetri) Paketi
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_FEEDBACK (Örn: 0x03)
    float    battery_voltage;// Lipo Pil Voltajı
    float    gyro_z;         // Aracın savrulma açısı
    uint8_t  car_status;     // 0: OK, 1: Low Battery, 2: Error
    int8_t   rssi;           // ESP-NOW Sinyal Gücü (dBm)
} car_feedback_packet_t;


TODO: ESP-NOW Packet Loss (Paket Kaybı) ve RSSI Monitörü:


TODO: WS2812B Arayüz


