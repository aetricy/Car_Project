# AVM (Alışveriş Merkezi) İçi Çoklu RC Araç & Simülatör Sistemi
## RF Girişim, Güvenlik ve Saha Kurulum Planı

Bu belge, bir alışveriş merkezinde (AVM) aynı anda çalışacak **6 adet Logitech G29 Direksiyon Seti (ESP32-S3)** ve **6 adet RC Aracın (ESP32-C3)** yüksek Wi-Fi yoğunluğu, metal/cam yansımaları ve kalabalık insan faktörü altında sıfır kesinti, minimum gecikme ve maksimum güvenlikle çalışabilmesi için hazırlanmış kapsamlı mühendislik ve operasyon rehberidir.

---

## 1. AVM Ortamındaki Kritik Zorluklar

Alışveriş merkezleri, 2.4 GHz radyo frekansı (RF) açısından en zorlu ve "kirli" ortamlardan biridir:

| Zorluk Faktörü | Mekanizma | Sistemimize Etkisi |
| :--- | :--- | :--- |
| **Aşırı Wi-Fi Doygunluğu** | Mağaza erişim noktaları (AP), kurumsal ağlar, POS cihazları ve ziyaretçilerin telefonlarından gelen saniyede binlerce Beacon ve Probe Request. | Kanal tıkanması, paket gecikmesi (jitter), paket kaybı ve ACK zaman aşımları. |
| **Çok Yollu Yansıma (Multipath)** | Metal tavan kirişleri, yürüyen merdivenler, kolonlar ve cam vitrinler sinyali yansıtıp farklı fazlarda alıcıya ulaştırır. | Faz çakışması nedeniyle anlık sinyal sönümlenmesi (fading) ve kör noktalar (dead spots). |
| **İnsan Gövdesi Zayıflatması** | İnsan vücudunun %70'i sudur. 2.4 GHz radyo dalgaları su molekülleri tarafından güçlü şekilde emilir. | Pist ile sürücü kabinleri arasına giren kalabalık sinyali **3 dB ile 10 dB** arası zayıflatır. |
| **Endüktif & Manyetik Gürültü** | RC araçlardaki yüksek akımlı fırçasız (brushless) motorlar, ESC ve servo motorlar. | ESP32-C3'ün güç hattında ve RF alıcı katında gürültü oluşturarak menzili kısaltır. |

---

## 2. Yazılımsal & İletişim Protokolü Önlemleri

### 2.1. Dinamik veya Ön Tanımlı Kanal Dağılımı (Kanal 1, 6, 11 ve 13)
* **Kural:** 6 aracı asla tek bir kanala (örn. Kanal 1) toplamayın.
* 2.4 GHz spektrumunda birbirini örtmeyen (non-overlapping) kanallar kullanılır.
* **Tavsiye Edilen Dağıtım:**
  * **Kanal 1:** Araç 1 & Araç 2
  * **Kanal 6:** Araç 3 & Araç 4
  * **Kanal 11:** Araç 5
  * **Kanal 13 (Türkiye/Avrupa ETSI):** Araç 6 *(Kanal 13 ticari AVM yönlendiricileri tarafından genellikle boş bırakılır, en temiz kanaldır).*
* **Saha Spektrum Taraması (Site Survey):** Kurulum günü pist alanında bir Wi-Fi Analyzer (veya ESP32 tarayıcısı) çalıştırılarak AVM'nin en az kullandığı 3 kanal tespit edilir ve sistem bu kanallara ayarlanır.

### 2.2. ESP-NOW PHY Hızını Artırma (Airtime Minimizasyonu)
* Varsayılan 1 Mbps DSSS aktarım hızı yerine **6 Mbps veya 12 Mbps OFDM** kullanılmalıdır:
  ```c
  // Hem S3 hem C3 tarafında Wi-Fi başlatıldıktan sonra:
  esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_6M);
  ```
* **Kazanım:**
  * Paketin havada kalma süresi (airtime) ~10 kat azalır.
  * OFDM modülasyonu, AVM içindeki metalik yansımalara (multipath) karşı 1 Mbps DSSS'ye göre çok daha dirençlidir.

### 2.3. Benzersiz Eşleşme Anahtarı (Pairing Token / Cross-Talk Önleme)
* Her kumanda-araç çiftine rastgele 16-bit bir `pairing_token` atanır.
* Araç, gelen sürüş paketinde token uyuşmazlığı varsa paketi anında düşürür.
* Böylece başka bir kumandanın yanlışlıkla araca müdahale etmesi matematiksel olarak imkansız hale gelir.

### 2.4. Yumuşatılmış Failsafe & Girdi Enterpolasyonu (Jitter Smoothing)
* Kalabalık RF ortamında 1-2 paketin kaybolması normaldir.
* Araç tarafında (C3) 1 paket kaçırıldığında son bilinen değer 40-60 ms korunur (Interpolation).
* Sinyal kesintisi **300 ms**'yi aştığında kademeli frenleme ve nötr pozisyona geçiş (Failsafe) uygulanır.

---

## 3. Donanım & Anten Altyapısı

Dahili zikzak PCB antenler ev ortamında yeterlidir ancak AVM gibi yoğun ortamlarda kesinlikle **harici antenli modüller** kullanılmalıdır.

```
+-------------------------------------------------------------------+
|                        GÖRÜŞ HATTI (LOS) DÜZENİ                   |
|                                                                   |
|   [ S3 Sürücü İstasyonları ]               [ RC Pist Alanı ]      |
|    Direksiyon Masası (1m)                     Zemin (0m)          |
|            |                                       |              |
|     (Uzatma Kablosu)                               |              |
|            |                                       |              |
|   [Harici SMA Anten] (2.2m Yükseklikte)            |              |
|            \                                      /               |
|             \====== TEMİZ GÖRÜŞ HATTI (LOS) =====/                |
|                     (Kalabalığın Baş Üstünden)                    |
|                                                    |              |
|                                            [ESP32-C3 Araç]        |
|                                            (Dikey Çubuk Anten)    |
+-------------------------------------------------------------------+
```

### 3.1. Anten Seçimi ve Montajı
1. **S3 Verici Tarafı (Sürücü Kabinleri):**
   * ESP32-S3 modüllerinde IPEX/U.FL konnektörlü modeller tercih edilmelidir.
   * Direksiyon setlerinin arkasına değil, **masanın üstüne veya kabin direğinin tepesine (yerden 2 - 2.5 metre yüksekliğe)** uzatma SMA kablosu ile 5 dBi Omni-direksiyonel antenler yerleştirilmelidir.
   * Bu sayede pist çevresinde toplanan izleyicilerin bedenleri sürücü ile araç arasındaki doğrudan sinyali kesemez (Line of Sight korunur).
2. **C3 Alıcı Tarafı (RC Araç):**
   * Küçük esnek çubuk anten (Coaxial Dipole / SMA) araç kepinin dışına dikey olarak çıkartılmalıdır.
   * Anten ucu karbon fiber şase plakalarından ve fırçasız motor güç kablolarından en az 3-4 cm uzakta tutulmalıdır.

### 3.2. Araç İçi Güç Filtreleme (Brown-out Önlemi)
* Güçlü direksiyon servoları ani dönüşlerde ESC dahili BEC voltajını 5V'tan 3.8V'a anlık düşürebilir.
* ESP32-C3 besleme pinlerine (5V/3.3V) paralel **470 µF – 1000 µF Düşük ESR Elektrolitik Kondansatör (Glitch Buster)** eklenmelidir. Bu, mikrodenetleyicinin resetlenmesini kesin olarak engeller.

---

## 4. Güvenlik & Operasyonel Önlemler

### 4.1. Hakem / Acil Durdurma Kumandası (Master E-Stop)
* Tüm araçların dinlediği global bir acil durdurma yayın kanalı (Broadcast Packet ID: `0xFF`) olmalıdır.
* Pist görevlisinin elindeki acil durdurma butonuna basıldığında tüm 6 araç 50 ms içinde motorlarını kesip sert fren moduna geçer.

### 4.2. Kademeli Hız & Güç Kısıtlaması (EPA Limitleri)
* AVM pistlerinde zemin genellikle kaygan epoksi, laminat veya halıdır.
* G29 üzerinden gaz EPA'sı yazılımsal olarak sınırlandırılabilir (örn. Çocuk/Acemi modu: %40 gaz, İleri mod: %75 gaz). Bu, araçların bariyerleri aşıp seyircilerin arasına dalma riskini ortadan kaldırır.

### 4.3. Fiziksel Pist Güvenliği
* Pist kenarlarına en az 30-40 cm yüksekliğinde polikarbonat (şeffaf pleksi) veya esnek sünger bariyer çekilmelidir.
* Araç kepinin burun ve yan kısımlarına yumuşak köpük tamponlar (foam bumper) takılmalıdır.

---

## 5. Uygulama Yol Haritası (Checklist)

- [ ] **Aşama 1: Harici Anten Donanımına Geçiş**
  - ESP32-S3 ve C3 modüllerini harici IPEX/SMA antenli tiplerle güncelleme.
- [ ] **Aşama 2: Çoklu Kanal Desteğinin Koda Eklenmesi**
  - S3 Dev Mode menüsüne "Wi-Fi Kanalı (1, 6, 11, 13)" seçim parametresi ekleme.
  - C3 alıcısına açılışta kanalı NVS'den okuma veya otomatik eşleme (Bind) özelliği kazandırma.
- [ ] **Aşama 3: Yüksek Hızlı PHY Modu (6 Mbps OFDM)**
  - `esp_wifi_config_espnow_rate()` entegrasyonu ile havada kalma süresinin düşürülmesi.
- [ ] **Aşama 4: Master Acil Durdurma (E-Stop)**
  - Bağımsız hakem güvenlik butonunun sisteme tanıtılması.
- [ ] **Aşama 5: Saha Testi (AVM Akustik ve RF Denemesi)**
  - Hafta sonu yoğun AVM saatinde alanda 6 araçla 15 dakikalık stres ve paket kaybı testi.

