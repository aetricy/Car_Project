TODO: Bidirectional Telemetry

// C3 -> S3 Feedback (Telemetry) Packet
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_FEEDBACK (e.g. 0x03)
    float    battery_voltage;// LiPo Battery Voltage
    float    gyro_z;         // Vehicle yaw rate / slip angle
    uint8_t  car_status;     // 0: OK, 1: Low Battery, 2: Error
    int8_t   rssi;           // ESP-NOW Signal Strength (dBm)
} car_feedback_packet_t;


TODO: ESP-NOW Packet Loss and RSSI Monitor:


TODO: WS2812B Interface


