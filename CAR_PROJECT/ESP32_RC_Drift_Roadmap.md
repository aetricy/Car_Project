---
tags: [esp32, freertos, esp-now, rc-drift, logitech-g29, kicad, embedded]
aliases: [Sim-to-Reality RC Roadmap, ESP32 Drift Araç Sistemi]
date_created: 2026-08-26
---

# 🏎️ ESP32 Sim-to-Reality RC Drift Telemetri & Kontrol Sistemi

Bu not, ESP32-S3 (Verici/G29 Host) ve ESP32-C3 (Alıcı/Araç) arasında kurulan sıfır gecikmeli, RTOS tabanlı haberleşme altyapısının fiziksel donanıma ve otonom sistemlere dönüştürülmesi için gereken mühendislik yol haritasını içerir. Şase hedefi: 1/24 & 1/28 RWD (Örn: TG Super TT).

---

## 📍 FAZ 1: Fiziksel Katman ve Aktüatör Sürüşü
Haberleşme katmanı tamamlandı. Bu fazda, havadan gelen dijital veriler fiziksel harekete (PWM) dönüştürülecek.

> [!warning] Donanım Uyarısı: 3.3V / 5V Toleransı
> ESP32-C3'ün GPIO pinleri 5V toleranslı **değildir**. ESC veya yüksek güçlü servolardan gelebilecek sinyal gürültüleri veya 5V geri beslemeleri için araya mutlaka Logic Level Converter (Seviye Dönüştürücü) veya koruyucu direnç eklenmelidir.

- [ ] **Donanımsal PWM Üretimi (MCPWM)**
  - [ ] ESP-IDF `mcpwm` veya `ledc` çevre birimleri kullanılarak 50Hz (20ms) periyot ayarlanacak.
  - [ ] Duty cycle aralığı RC standartlarına (1000µs - 2000µs, merkez 1500µs) kalibre edilecek.
- [ ] **Matematiksel Haritalama (Mapping)**
  - [ ] G29'dan gelen ham veri (8-bit veya 14-bit) PWM mikrosaniye değerlerine dönüştürülecek.
  - [ ] Titremeyi (jitter) önlemek için kayan nokta (float) yerine sabit nokta (fixed-point) matematiği kullanılacak.
- [ ] **Sürüş Dinamikleri (Yazılımsal)**
  - [ ] Direksiyon için "Expo (Eksponansiyel)" algoritması yazılacak (Merkezde hassas, kenarlarda agresif tepki).
  - [ ] Gaz ve fren için "Deadzone (Ölü bölge)" ayarları yapılacak.


---

## 📍 FAZ 2: Çift Yönlü Zaman Bölmeli Telemetri (TDM)
Sistem sadece emir almamalı, aracın durumunu S3'e (G29'a) geri bildirmelidir.

> [!danger] RF Çakışması (Collision)
> S3 saniyede 50 kere veri fırlatırken, C3 de kendi verisini bağımsızca fırlatmaya kalkarsa Wi-Fi bandında çakışmalar (ESP_ERR_ESPNOW_NO_MEM) başlar. Rastgele gönderim yapılmamalıdır.

- [ ] **TDM (Time-Division Multiplexing) Mimarisi Kurulumu**
  - [ ] C3, sadece S3'ten paket geldiği an (RX Callback içinde bayrak kaldırarak) kendi telemetri paketini "ACK/Cevap" olarak gönderecek şekilde senkronize edilecek.
- [ ] **Araç Verilerinin Okunması (C3 Tarafı)**
  - [ ] ADC kullanılarak Li-Po bataryanın anlık voltaj okuması yapılacak (Gerilim bölücü direnç ağı ile).
  - [ ] Gyro'dan alınan anlık kayma açısı (Slip angle) telemetri paketine eklenecek.
- [ ] **Sürücü Geri Bildirimi (S3 Tarafı)**
  - [ ] Gelen batarya verisi belli bir eşiğin altındaysa G29 üzerindeki RPM LED'leri (veya harici bir buzzer) ile uyarı verdirilecek.

---

## 📍 FAZ 3: G29 Force Feedback (FFB) Entegrasyonu
Projenin zirve noktası. Araçtaki fiziksel olayların G29'un motorlarına aktarılarak gerçekçi simülasyon hissinin yaratılması.

> [!todo] Araştırma Konusu
> Logitech G29'un USB FFB Report Descriptor'larının (hangi baytın ne kadar tork veya titreşim ürettiğinin) tersine mühendislikle (Reverse Engineering) çözülmesi veya açık kaynak kütüphanelerden (örneğin Linux hid-logitech-wheel sürücülerinden) port edilmesi gerekiyor.

- [ ] **USB HID Çıkış (OUT) Raporlarının Yönetimi**
  - [ ] S3 üzerinde USB Host sürücüsüne FFB raporlarını gönderecek modül yazılacak.
- [ ] **Fizik Motoru (S3 Tarafı)**
  - [ ] Araçtan (C3'ten) gelen Yaw ivmesi ve RPM verilerine göre direksiyona ters tork (Self-aligning torque) uygulayacak matematiksel model yazılacak.
  - [ ] *Gecikme Optimizasyonu:* Aracın kayması ile direksiyondaki tepki süresi 15ms'nin altında tutulacak.

