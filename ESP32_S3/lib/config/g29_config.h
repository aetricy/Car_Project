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



// --- PAKET TİPLERİ (OPCODE) ---
// S3'ün C3'e ne tarz bir mesaj gönderdiğini belirten bayrak
#define PKT_TYPE_DRIVE      0x01  // Normal anlık sürüş verisi (Saniyede 50 kez akar)
#define PKT_TYPE_COMMAND    0x02  // Sistem komutları (Uyku, Failsafe, Mod değişimi vb.)
#define PKT_TYPE_FEEDBACK   0x03  

// --- SİSTEM KOMUTLARI (COMMAND ID) ---
#define CMD_SLEEP_ENTER     0x10  // S3 uykuya geçti, C3 de uykuya geçsin
#define CMD_WAKE_UP         0x11  // S3 uyandı, C3 de uyanık moda geçsin
#define CMD_FAILSAFE_STOP   0xEE  // Acil Durdurma (Kablo koptu / S3 kapandı)

// ==========================================
// 1. S3 -> C3 GÖNDERİLEN HAFİF SÜRÜŞ PAKETİ (Sadece Araba İçin Gerekenler)
// ==========================================
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_DRIVE (0x01)
    uint8_t  packet_id;      // Paket Sayacı (Kayıp tespiti için)
    
    // Araba için SADECE motor ve direksiyon yeterlidir (Debriyaj/Butonlar uçuruldu)
    int8_t   steering;       // -128 ile +127 arası sıkıştırılmış direksiyon (Bant genişliği tasarrufu)
    uint8_t  throttle;       // 0 ile 255 arası gaz
    uint8_t  brake;          // 0 ile 255 arası fren
} car_drive_packet_t;

// ==========================================
// 2. S3 -> C3 DURUM VE KOMUT PAKETİ (Hata / Uyku / Mod)
// ==========================================
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_COMMAND (0x02)
    uint8_t  command_id;     // CMD_SLEEP_ENTER, CMD_FAILSAFE_STOP vb.
    uint8_t  parameter;      // Ek veri (Gerekirse mod ID'si vb. taşımak için)
} car_command_packet_t;

// ==========================================
// 3. S3 KENDİ İÇİNDE KULLANDIĞI TAM G29 TELEMETRİSİ (Değişmedi)
// ==========================================
typedef struct {
    float steering;          // -1.0 ile 1.0
    float throttle;          // 0.0 ile 1.0
    float brake;             // 0.0 ile 1.0
    float clutch;            // 0.0 ile 1.0
    uint16_t buttons_state;  // Tüm butonlar (LED, Ekran ve UI için S3 içinde kalır)
} g29_telemetry_t;


// C3 -> S3 Geri Bildirim (Telemetri) Paketi
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_FEEDBACK (Örn: 0x03)
    float    battery_voltage;// Lipo Pil Voltajı
    float    gyro_z;         // Aracın savrulma açısı
    uint8_t  car_status;     // 0: OK, 1: Low Battery, 2: Error
    int8_t   rssi;           // ESP-NOW Sinyal Gücü (dBm)
} car_feedback_packet_t;

#endif