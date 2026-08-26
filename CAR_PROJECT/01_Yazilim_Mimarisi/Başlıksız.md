---
tags: [master-doc, esp32, freertos, rc-drift, esp-now]
aliases: [Tüm Sistem Kodları ve Mimari]
date: 2026-08-26
---

# 🏎️ ESP32 Sim-to-Reality RC Drift Projesi (Mega Master Döküman)

Bu döküman, Logitech G29 direksiyon setinden alınan yüksek frekanslı USB HID verilerini; sıfır gecikme (zero-latency), %100 kararlılık ve akıllı uyku (Smart Sleep) algoritmalarıyla 1/24 & 1/28 ölçekli RC Drift aracına aktaran sistemin **tüm mimarisini, yol haritasını ve nihai kodlarını** içerir.

---

## 📌 BÖLÜM 1: SİSTEM MİMARİSİ VE VERİ AKIŞI

Sistemin temel felsefesi: **"Sıfır Gecikme, Olay Güdümlü (Event-Driven) Çalışma ve Çekirdek Soyutlama (Decoupling)"**






## 🚀 BÖLÜM 2: MÜHENDİSLİK YOL HARİTASI (ROADMAP)

Donanım ve otonom sistem entegrasyonu için izlenecek aşamalar:

> [!warning] Donanım Uyarısı: 3.3V / 5V Toleransı ESP32-C3'ün pinleri 5V toleranslı **değildir**. ESC veya servolardan gelecek sinyal gürültüleri/5V geri beslemeleri için araya mutlaka Seviye Dönüştürücü (Logic Level Converter) veya seri direnç eklenmelidir.

- [ ] **FAZ 1: Fiziksel Katman (MCPWM)**
    
    - ESP-IDF `mcpwm` donanımı ile 50Hz (20ms periyot, 1000µs - 2000µs) duty cycle üretimi.
        
    - Sürüş dinamikleri için Float'tan Fixed-Point'e dönüşüm ve "Expo" direksiyon algoritmaları.
        
- [ ] **FAZ 2: Otonom Destek (Drift Assist / Gyro)**
    
    - C3'e I2C üzerinden Gyro (MPU6050/BMI270) entegrasyonu.
        
    - Aracın Yaw Rate değerini okuyup servoya Counter-steer (ters açı) verecek **PID Kontrolcüsü**.
        
    - S3'ten gelen verinin doğrudan servo açısı değil, PID için "Hedef Açı (Set-Point)" olarak kullanılması.
        
- [ ] **FAZ 3: Çift Yönlü Telemetri (TDM Mimarisi)**
    
    - RF çarpışmalarını (Collision) önlemek için Time-Division Multiplexing (TDM) kullanımı (S3 paket attığı an C3'ün cevap vermesi).
        
    - C3'ten G29 RPM ledlerine Li-Po batarya durumu geri bildirimi.
        
- [ ] **FAZ 4: G29 Force Feedback (FFB)**
    
    - Araçtaki kayma ivmesinin S3 tarafından hesaplanıp G29 motorlarına ters tork (Self-aligning torque) olarak iletilmesi.
        
- [ ] **FAZ 5: PCB Tasarımı (KiCad)**
    
    - 1/28 şaseler için ağırlık merkezini bozmayacak 15x25mm boyutlarında, RF (Anten) kurallarına uygun özel "Alıcı + Gyro + Işık" PCB dizaynı.
        

## 💻 BÖLÜM 3: VERİCİ (ESP32-S3) KODLARI

**Amaç:** USB'den gelen 1000Hz'lik paketleri süzmek, Wi-Fi'yi boğmamak için 50Hz hızında sabitlemek ve hareketsizlik anında uykuya geçmek.

### 📄 `esp_now_sender.h`

```
#ifndef ESP_NOW_SENDER_H
#define ESP_NOW_SENDER_H
#include "g29_processor.h"

void init_esp_now_sender(void);
void send_telemetry_to_car(const g29_telemetry_t *telemetry);

#endif
```

### 📄 `esp_now_sender.c`

```
#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_timer.h" 

#include "esp_now_sender.h"
#include "g29_config.h"
#include "config.h"

static const char *TAG = "ESP_NOW_SENDER";
uint8_t target_car_mac[6];

static SemaphoreHandle_t telemetry_mutex = NULL;
static TaskHandle_t espnow_tx_task_handle = NULL;
static g29_telemetry_t shared_telemetry;
static volatile bool is_sleeping = true; 

extern volatile int64_t last_g29_input_time;
extern const int64_t INACTIVITY_TIMEOUT_US;

static void esp_now_sender_task(void *arg) {
    g29_telemetry_t packet;
    
    while (1) {
        int64_t current_time = esp_timer_get_time();

        if ((current_time - last_g29_input_time) < INACTIVITY_TIMEOUT_US) {
            is_sleeping = false; 

            if (xSemaphoreTake(telemetry_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                packet = shared_telemetry;
                xSemaphoreGive(telemetry_mutex);
                esp_now_send(target_car_mac, (uint8_t *)&packet, sizeof(g29_telemetry_t));
            }

            vTaskDelay(pdMS_TO_TICKS(20)); // Sıkı 50Hz kilidi
            ulTaskNotifyTake(pdTRUE, 0);   // Biriken sinyalleri yut
        } 
        else {
            if (!is_sleeping) {
                ESP_LOGW(TAG, "30 Saniye hareketsizlik. Uykuya geciliyor...");
                is_sleeping = true;
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Süresiz Uyku
            ESP_LOGI(TAG, "Hareket algilandi! 50Hz yayin tekrar basliyor.");
        }
    }
}

void init_esp_now_sender(void) {
    memcpy(target_car_mac, CAR_MAC_TABLE[ACTIVE_CAR_ID], 6);
    telemetry_mutex = xSemaphoreCreateMutex();

    ESP_ERROR_CHECK(nvs_flash_init()); // Basitleştirildi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // CRASH ÖNLEYİCİ

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, target_car_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    xTaskCreatePinnedToCore(
        esp_now_sender_task, "esp_now_sender", 4096, NULL, 4, 
        &espnow_tx_task_handle, OTHER_TASK_CORE        
    );
}

void send_telemetry_to_car(const g29_telemetry_t *telemetry) {
    if (telemetry_mutex != NULL) {
        if (xSemaphoreTake(telemetry_mutex, portMAX_DELAY) == pdTRUE) {
            shared_telemetry = *telemetry;
            xSemaphoreGive(telemetry_mutex);
        }
        if (is_sleeping && espnow_tx_task_handle != NULL) {
            xTaskNotifyGive(espnow_tx_task_handle); // Sadece uyuyorsa uyandır
        }
    }
}
```

### 📄 `app_main.c` (S3 - Ana Giriş)

```
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h" 
#include "g29_driver_host.h" 
#include "esp_now_sender.h"

static const char *TAG = "MAIN_APP";
volatile int64_t last_g29_input_time = 0; 
const int64_t INACTIVITY_TIMEOUT_US = 30000000; // 30 Saniye

g29_telemetry_t current_telemetry;

void on_g29_input_received(const uint8_t *data, int len) {
    if(g29_is_ready()){
        last_g29_input_time = esp_timer_get_time(); // Zamanı sıfırla
        g29_process_raw_data(data, len, &current_telemetry);
        send_telemetry_to_car(&current_telemetry); 
    }
}

void on_g29_state_changed(g29_state_t state) {
    if (state == G29_STATE_NATIVE_READY) {
        ESP_LOGI(TAG, "G29 HAZIR!");
        g29_disable_autocenter();
        g29_set_range(540);
    }
}

void app_main(void) {
    init_esp_now_sender(); // Önce Wi-Fi ve ESP-NOW Kurulur
    if (g29_init(on_g29_state_changed, on_g29_input_received) == ESP_OK) {
        ESP_LOGI(TAG, "Sürücü Başarıyla Kuruldu. USB Bekleniyor...");
    }
}
```

## 📡 BÖLÜM 4: ALICI (ESP32-C3) KODLARI

**Amaç:** "Getter API" ile kapsülleme (Encapsulation) kullanarak Wi-Fi kesmesini hafifletmek ve motor kontrol döngüsünde sıfır gecikme sağlamak.

### 📄 `esp_now_receiver.h`

```
#ifndef ESP_NOW_RECEIVER_H
#define ESP_NOW_RECEIVER_H
#include <stdbool.h>
#include "g29_config.h" 

void init_esp_now_receiver(void);
bool esp_now_get_latest_data(g29_telemetry_t *out_data);

#endif
```

### 📄 `esp_now_receiver.c`

```
#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_now_receiver.h"

static g29_telemetry_t latest_telemetry;
static volatile bool new_data_available = false;

// KESME (ISR): SADECE KOPYALA VE ÇIK
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(g29_telemetry_t)) {
        memcpy(&latest_telemetry, data, sizeof(g29_telemetry_t));
        new_data_available = true;
    }
}

void init_esp_now_receiver(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));
}

bool esp_now_get_latest_data(g29_telemetry_t *out_data) {
    if (new_data_available) {
        memcpy(out_data, &latest_telemetry, sizeof(g29_telemetry_t));
        new_data_available = false;
        return true;
    }
    return false;
}
```

### 📄 `app_main.c` (C3 - Ana Giriş)

```
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "g29_config.h"
#include "esp_now_receiver.h" 

void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    init_esp_now_receiver();
    printf("ESP32-C3 Dinlemede... S3 Verisi Bekleniyor.\n");

    g29_telemetry_t current_telemetry;

    while (1) {
        // Getter API ile yeni veri sorgusu (Non-blocking)
        if (esp_now_get_latest_data(&current_telemetry)) {
            printf("Steering: %5.2f | Throttle: %4.2f\n", 
                     current_telemetry.steering, 
                     current_telemetry.throttle);
                     
            // TODO: MCPWM / Servo kodları buraya yazılacak.
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Döngüyü rahatlat
    }
}
```

## 🐞 BÖLÜM 5: HATA AYIKLAMA GÜNLÜĞÜ (BUG LOG)

Bu bölümde, projenin geliştirilmesi sırasında karşılaşılan kronik hatalar ve çözümleri arşivlenmiştir.

> [!check] BUG 01: Wi-Fi Kuyruk Şişmesi (0.70'te Takılma ve Hata 12391) **Neden:** `xQueue` kullanıldığında USB'den gelen 1000Hz veri, Wi-Fi'nin gönderim hızını aştığı için TX Buffer taşıyordu. Araç geçmiş hareketleri yansıtıyordu. **Çözüm:** Kuyruk mimarisi iptal edildi. `xSemaphoreTake` (Mutex) kullanılarak sadece o anki en güncel değer (snapshot) RAM'den alınıp tam 50Hz ritmiyle gönderilecek şekilde Event-Driven mimariye geçildi.

> [!check] BUG 02: Core 1 LoadProhibited Panik Çökmesi **Neden:** `esp_now_init()` fonksiyonları USB Interrupt (ISR) içerisinde çağrıldı ve Wi-Fi başlatılırken `esp_event_loop_create_default()` eksik bırakıldığı için sistem NULL pointer'a erişip çöktü. **Çözüm:** Kurulum sırası `app_main` içine taşındı ve eksik event loop başlatma satırı eklendi.

> [!check] BUG 03: Alıcı (C3) Tarafındaki Ekran Gecikmesi (Latency) **Neden:** ESP-NOW alıcı kesmesi (RX Callback) içinde yavaş bir UART işlemi olan `printf` kullanılıyordu. **Çözüm:** "Kapsülleme (Encapsulation)" mimarisi kurgulandı. Callback içinde sadece `memcpy` yapılıp çıkılıyor, ekrana yazdırma ve servo sürme işlemleri Getter API aracılığıyla C3'ün ana döngüsüne bırakıldı. Gecikme 0ms'ye indi.