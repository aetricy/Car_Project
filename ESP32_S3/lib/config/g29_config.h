#ifndef G29_CONFIG_H
#define G29_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define G29_RANGE                   900
#define G29_AUTOCENTER_STRENGHT     0
#define G29_AUTOCENTER_RATE         0

#define STEERING_DEADZONE           0.02f // %2'lik merkez ölü bölge (Direksiyon boşluğunu alır)
#define PEDAL_DEADZONE              0.05f      

#define LOGIWHEEL_BTN_PLUS          0x80  // 3.byte 1000 0000
#define LOGIWHEEL_BTN_MINUS         0x01  // 2.byte 

// --- PAKET TİPLERİ (OPCODE) ---
// S3'ün C3'e ne tarz bir mesaj gönderdiğini belirten bayrak
#define PKT_TYPE_DRIVE      0x01  // Normal anlık sürüş verisi (1000-2000 PWM)
#define PKT_TYPE_COMMAND    0x02  // Sistem komutları (Uyku, Failsafe, Mod değişimi vb.)
#define PKT_TYPE_CONFIG     0x03  // Sadece ayar değiştiğinde NVS'e yazılacak veriler

// --- SİSTEM KOMUTLARI (COMMAND ID) ---
#define CMD_SLEEP_ENTER     0x10  // S3 uykuya geçti, C3 de uykuya geçsin
#define CMD_WAKE_UP         0x11  // S3 uyandı, C3 de uyanık moda geçsin
#define CMD_FAILSAFE_STOP   0xEE  // Acil Durdurma (Kablo koptu / S3 kapandı)

// ==========================================
// 1. S3 -> C3 GÖNDERİLEN SÜRÜŞ PAKETİ (Anlık Akar)
// ==========================================
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;    // PKT_TYPE_DRIVE (0x01)
    uint8_t  packet_id;     
    
    // PWM formatında gönderilecek (1000-2000 arası, Merkez: 1500)
    uint16_t steering;       // 1000 (Tam Sol) - 2000 (Tam Sağ)
    uint16_t throttle;       // 1000 (Tam Geri) - 2000 (Tam İleri)
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
// 3. S3 -> C3 AYAR (CONFIG) PAKETİ (Sadece değişince gider)
// ==========================================
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;     // PKT_TYPE_CONFIG (0x03)
    
    // General Ayarlar
    uint8_t  st_gyro_gain; // -100 100

    // Direksiyon (Steering) Ayarları
    int8_t   st_sub_trim;     // Merkez kaydırma (-100 to 100)
    uint8_t  st_epa_left;     // Sol End Point (0-100%)
    uint8_t  st_epa_right;    // Sağ End Point (0-100%)
    bool     st_reverse;      // Ters yön (true/false)
    uint8_t  st_curve;        // Eğri tipi (0: Lineer, 1: Expo vb.)
    
    // Gaz (Throttle) Ayarları
    int8_t   th_sub_trim;     // Merkez kaydırma (-100 to 100)
    uint8_t  th_epa_forward;  // İleri End Point (0-100%)
    uint8_t  th_epa_backward; // Geri End Point (0-100%)
    bool     th_reverse;      // Ters yön (true/false)
    uint8_t  th_curve;        // Eğri tipi (0: Lineer, 1: Expo vb.)
} car_config_packet_t;

// ==========================================
// 4. S3 KENDİ İÇİNDE KULLANDIĞI TAM G29 TELEMETRİSİ (ESP-NOW'a gitmez)
// ==========================================
typedef struct {
    float steering;          // -1.0 ile 1.0
    float throttle;          // 0.0 ile 1.0
    float brake;             // 0.0 ile 1.0
    float clutch;            // 0.0 ile 1.0
    uint16_t buttons_state;  // Tüm butonlar (LED, Ekran ve UI için S3 içinde kalır)
} g29_telemetry_t;

// --- GÖNDERİCİ TX KAPSAYICI (UNION) ---
typedef struct {
    size_t length; // Havaya fırlatılacak gerçek boyut
    union {
        car_drive_packet_t   drive;
        car_command_packet_t command;
        car_config_packet_t  config;
    } payload;
} espnow_tx_item_t;

#endif