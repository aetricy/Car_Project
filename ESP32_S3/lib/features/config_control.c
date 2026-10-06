#include "config_control.h"
#include "led_ui_on_g29.h"
#include "g29_button_map.h"
#include "CONFIG.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "DEV_CONFIG";

#define NVS_NAMESPACE_S3_CFG "s3_cfg_ns"
#define NVS_KEY_S3_CFG       "active_cfg"

// Aktif ayar verisi
static car_config_packet_t g_active_config;

// Dev Mode Durumu
static bool s_dev_mode_active = false;
static cfg_menu_t s_current_menu = CFG_MENU_ST_EPA;

// NVS Aşınma Koruması Değişkenleri
static bool s_config_dirty = false;
static TickType_t s_last_config_change_tick = 0;

// Zamanlayıcılar ve Tuş Durumları
static TickType_t s_value_display_until = 0;
static uint32_t s_last_buttons = 0;
static uint32_t s_combo_hold_ticks = 0;
static bool s_combo_latched = false;
static uint32_t s_enter_hold_ticks = 0;
static bool s_enter_latched = false;

extern volatile s3_logic_state_t current_system_state;
extern QueueHandle_t espnow_tx_queue;

static void send_save_cmd_to_car(void) {
    if (espnow_tx_queue != NULL) {
        espnow_tx_item_t cmd_item;
        memset(&cmd_item, 0, sizeof(espnow_tx_item_t));
        cmd_item.length = sizeof(car_command_packet_t);
        cmd_item.payload.command.packet_type = PKT_TYPE_COMMAND;
        cmd_item.payload.command.command_id  = CMD_CONFIG_SAVE;
        cmd_item.payload.command.parameter   = 0;
        xQueueSend(espnow_tx_queue, &cmd_item, 0);
        ESP_LOGI(TAG, "Araca CMD_CONFIG_SAVE komutu gonderildi.");
    }
}

static bool is_config_valid(const car_config_packet_t *cfg) {
    if (!cfg) return false;
    if (cfg->st_epa_left < 20 || cfg->st_epa_left > 100) return false;
    if (cfg->st_epa_right < 20 || cfg->st_epa_right > 100) return false;
    if (cfg->st_curve > 2) return false;
    if (cfg->st_sub_trim < -50 || cfg->st_sub_trim > 50) return false;
    if (cfg->th_epa_forward < 20 || cfg->th_epa_forward > 100) return false;
    if (cfg->th_epa_backward < 20 || cfg->th_epa_backward > 100) return false;
    if (cfg->th_curve > 2) return false;
    if (cfg->th_sub_trim < -50 || cfg->th_sub_trim > 50) return false;
    if (cfg->st_gyro_gain < 0 || cfg->st_gyro_gain > 100) return false;
    return true;
}

bool config_control_save_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_S3_CFG, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "S3 NVS acilamadi: %s", esp_err_to_name(err));
        return false;
    }

    // Flash yıpranmasını önlemek için: Mevcut kayıtlı veriyi oku ve karşılaştır
    car_config_packet_t existing_cfg;
    size_t req_len = sizeof(car_config_packet_t);
    err = nvs_get_blob(handle, NVS_KEY_S3_CFG, &existing_cfg, &req_len);
    if (err == ESP_OK && req_len == sizeof(car_config_packet_t)) {
        if (memcmp(&existing_cfg, &g_active_config, sizeof(car_config_packet_t)) == 0) {
            nvs_close(handle);
            ESP_LOGI(TAG, "S3 NVS: Veriler ayni, flash yazimi atlandi (Omur korundu).");
            return true;
        }
    }

    err = nvs_set_blob(handle, NVS_KEY_S3_CFG, &g_active_config, sizeof(car_config_packet_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "S3 NVS: Guncel ayarlar basariyla Flash NVS'e yazildi ve commit edildi.");
        } else {
            ESP_LOGE(TAG, "S3 NVS commit hatasi: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "S3 NVS set_blob hatasi: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    return (err == ESP_OK);
}

bool config_control_load_from_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_S3_CFG, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    car_config_packet_t loaded;
    size_t req_len = sizeof(car_config_packet_t);
    err = nvs_get_blob(handle, NVS_KEY_S3_CFG, &loaded, &req_len);
    nvs_close(handle);

    if (err == ESP_OK && req_len == sizeof(car_config_packet_t) && is_config_valid(&loaded)) {
        memcpy(&g_active_config, &loaded, sizeof(car_config_packet_t));
        return true;
    }

    return false;
}

void config_control_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (config_control_load_from_nvs()) {
        ESP_LOGI(TAG, "S3 NVS'ten kaydedilmis ayarlar basariyla yuklendi! (EPA Sol: %d%%, Sag: %d%%, Gyro: %d%%, Trim: %d)",
                 g_active_config.st_epa_left, g_active_config.st_epa_right, g_active_config.st_gyro_gain, g_active_config.st_sub_trim);
    } else {
        g_active_config.packet_type     = PKT_TYPE_CONFIG;
        g_active_config.st_gyro_gain    = 50; // Varsayilan %50 Gain (1500us)
        
        // Direksiyon Varsayılanları
        g_active_config.st_sub_trim     = 0;
        g_active_config.st_epa_left     = 100;
        g_active_config.st_epa_right    = 100;
        g_active_config.st_reverse      = false;
        g_active_config.st_curve        = 0; // 0: Lineer
        
        // Gaz Varsayılanları
        g_active_config.th_sub_trim     = 0;
        g_active_config.th_epa_forward  = 100;
        g_active_config.th_epa_backward = 100;
        g_active_config.th_reverse      = false;
        g_active_config.th_curve        = 0; // 0: Lineer

        config_control_save_to_nvs();
        ESP_LOGI(TAG, "Config Yoneticisi Baslatildi (Varsayilan degerler atandi ve NVS'e kaydedildi).");
    }

    s_dev_mode_active = false;
    s_current_menu = CFG_MENU_ST_EPA;
    s_value_display_until = 0;
    s_combo_hold_ticks = 0;
    s_combo_latched = false;
    s_config_dirty = false;
}

bool config_control_is_dev_mode(void) {
    return s_dev_mode_active;
}

void config_control_set_dev_mode(bool enable) {
    if (s_dev_mode_active == enable) return;

    s_dev_mode_active = enable;
    if (s_dev_mode_active) {
        current_system_state = STATE_DEV_MODE;
        ESP_LOGW(TAG, ">>> DEV MODE AKTIF EDILDI! G29 LED ve Tus Kontrolu Basladi <<<");
        g29_led_ui_intro_animation();
        s_current_menu = CFG_MENU_ST_EPA;
        s_value_display_until = 0;
    } else {
        current_system_state = STATE_SYS_ACTIVE;
        ESP_LOGW(TAG, ">>> DEV MODE KAPATILDI! Normal Surus Aktif <<<");
        
        // Dev Mode'dan cikildi: Eger ayarlar degistirilmisse NVS'e kaydet ve C3'e kaydet komutu yolla
        if (s_config_dirty) {
            config_control_save_to_nvs();
            send_save_cmd_to_car();
            s_config_dirty = false;
        }

        g29_led_ui_exit_animation();
        g29_led_ui_clear();
    }
}

const car_config_packet_t* config_control_get_active_config(void) {
    return &g_active_config;
}

// EPA Değerini 1-5 Bar Seviyesine Eşler
static uint8_t epa_to_bar_level(uint8_t epa) {
    if (epa <= 40) return 1;
    if (epa <= 55) return 2;
    if (epa <= 70) return 3;
    if (epa <= 85) return 4;
    return 5;
}

// Menü Değerini LED Barı Olarak Göster
static void update_led_display(const g29_telemetry_t *telemetry) {
    TickType_t now = xTaskGetTickCount();

    if (now < s_value_display_until) {
        switch (s_current_menu) {
            case CFG_MENU_ST_EPA: {
                uint8_t epa_val;
                if (telemetry->steering < -0.2f) {
                    epa_val = g_active_config.st_reverse ? g_active_config.st_epa_right : g_active_config.st_epa_left;
                } else if (telemetry->steering > 0.2f) {
                    epa_val = g_active_config.st_reverse ? g_active_config.st_epa_left : g_active_config.st_epa_right;
                } else {
                    epa_val = g_active_config.st_epa_left;
                }
                g29_led_ui_show_bar(epa_to_bar_level(epa_val));
                break;
            }
            case CFG_MENU_ST_CURVE:
                // Curve 0: 1 LED, Curve 1: 2 LED, Curve 2: 3 LED
                g29_led_ui_show_bar(g_active_config.st_curve + 1);
                break;

            case CFG_MENU_TH_EPA: {
                uint8_t epa_val;
                if (telemetry->brake > 0.2f) {
                    epa_val = g_active_config.th_reverse ? g_active_config.th_epa_forward : g_active_config.th_epa_backward;
                } else {
                    epa_val = g_active_config.th_reverse ? g_active_config.th_epa_backward : g_active_config.th_epa_forward;
                }
                g29_led_ui_show_bar(epa_to_bar_level(epa_val));
                break;
            }
            case CFG_MENU_TH_CURVE:
                g29_led_ui_show_bar(g_active_config.th_curve + 1);
                break;

            case CFG_MENU_ST_TRIM: {
                int8_t trim = g_active_config.st_sub_trim;
                int8_t pos = 0;
                if (trim <= -20) pos = -2;
                else if (trim < -5) pos = -1;
                else if (trim <= 5) pos = 0;
                else if (trim < 20) pos = 1;
                else pos = 2;
                g29_led_ui_show_trim_position(pos);
                break;
            }
            case CFG_MENU_GYRO_GAIN: {
                // Gyro gain %0 - %100 arası 1-5 bar olarak gösterilir
                uint8_t gain = g_active_config.st_gyro_gain;
                uint8_t level = 1;
                if (gain <= 20) level = 1;
                else if (gain <= 40) level = 2;
                else if (gain <= 60) level = 3;
                else if (gain <= 80) level = 4;
                else level = 5;
                g29_led_ui_show_bar(level);
                break;
            }
            default:
                break;
        }
    } else {
        // --- MENÜ SEÇİM MODU (Aktif Menü LED'i Yanıp Söner) ---
        bool blink_state = ((now / pdMS_TO_TICKS(250)) % 2) == 0;
        g29_led_ui_show_menu_single((uint8_t)s_current_menu, blink_state);
    }
}

bool config_control_process(const g29_telemetry_t *telemetry, car_config_packet_t *out_config_packet) {
    if (!telemetry) return false;

    uint32_t current_buttons = telemetry->buttons_state;
    uint32_t pressed = current_buttons & ~s_last_buttons; // Rising Edge
    s_last_buttons = current_buttons;

    bool config_changed = false;

    // ==============================================================
    // 1. DEV MODE AÇMA / KAPAMA KOMBİNASYONU (1.5 Saniye Basılı Tutma)
    //    OPTIONS + SHARE Birlikte VEYA Tek Başına PS Tuşu
    // ==============================================================
    bool combo_held = ((current_buttons & (BTN_SHARE | BTN_OPTIONS)) == (BTN_SHARE | BTN_OPTIONS)) ||
                      ((current_buttons & BTN_PS) != 0);

    if (combo_held) {
        if (!s_combo_latched) {
            s_combo_hold_ticks++;
            // 20ms * 75 = 1500ms (1.5 saniye)
            if (s_combo_hold_ticks >= 75) {
                config_control_set_dev_mode(!s_dev_mode_active);
                s_combo_latched = true; // Tekrar tetiklenmemesi için kilitle
                // Dev Mode'a girildiğinde ilk paketi hemen gönder
                if (s_dev_mode_active) {
                    config_changed = true;
                }
            }
        }
    } else {
        s_combo_hold_ticks = 0;
        s_combo_latched = false;
    }

    // Dev Mode Aktif Değilse Başka İşlem Yapma
    if (!s_dev_mode_active) {
        return false;
    }

    // ==============================================================
    // 2. MENÜ DEĞİŞTİRME (D-Pad Yukarı / Aşağı)
    // ==============================================================
    if (pressed & BTN_DPAD_UP) {
        s_current_menu = (cfg_menu_t)((s_current_menu + 1) % CFG_MENU_COUNT);
        s_value_display_until = 0; // Yeni menüye geçince menü LED'ini göster
        ESP_LOGI(TAG, "Menü Seçildi -> [%d] (1:ST_EPA, 2:ST_CURVE, 3:TH_EPA, 4:TH_CURVE, 5:TRIM, 6:GYRO)", s_current_menu + 1);
    } 
    else if (pressed & BTN_DPAD_DOWN) {
        s_current_menu = (cfg_menu_t)((s_current_menu + CFG_MENU_COUNT - 1) % CFG_MENU_COUNT);
        s_value_display_until = 0;
        ESP_LOGI(TAG, "Menü Seçildi -> [%d] (1:ST_EPA, 2:ST_CURVE, 3:TH_EPA, 4:TH_CURVE, 5:TRIM, 6:GYRO)", s_current_menu + 1);
    }

    // ==============================================================
    // 3. DEĞER ARTIRMA / AZALTMA (Dial YERİNE Tuşlar)
    //    - + / - Tuşları (Kırmızı çarkın üstündeki butonlar)
    //    - D-Pad Sol / Sağ (Yön tuşları)
    //    - Kulakçıklar: Sağ Kulakçık (+), Sol Kulakçık (-)
    // ==============================================================
    bool inc = (pressed & (BTN_PLUS  | BTN_DPAD_RIGHT | BTN_PADDLE_RIGHT)) != 0;
    bool dec = (pressed & (BTN_MINUS | BTN_DPAD_LEFT  | BTN_PADDLE_LEFT))  != 0;

    if (inc || dec) {
        s_value_display_until = xTaskGetTickCount() + pdMS_TO_TICKS(1800); // 1.8 sn değer çubuğunu göster
        config_changed = true;

        switch (s_current_menu) {
            
            // --- MENÜ 1: DİREKSİYON EPA ---
            case CFG_MENU_ST_EPA: {
                uint8_t *target_epa = NULL;
                const char *dir_name = NULL;

                if (telemetry->steering < -0.2f) {
                    // Direksiyon Sola Çevrili:
                    // Reverse aktifse araba sağa gideceğinden Sağ EPA, normalde Sol EPA seçilir
                    if (g_active_config.st_reverse) {
                        target_epa = &g_active_config.st_epa_right;
                        dir_name = "SAG (Reverse ile)";
                    } else {
                        target_epa = &g_active_config.st_epa_left;
                        dir_name = "SOL";
                    }
                } 
                else if (telemetry->steering > 0.2f) {
                    // Direksiyon Sağa Çevrili:
                    // Reverse aktifse araba sola gideceğinden Sol EPA, normalde Sağ EPA seçilir
                    if (g_active_config.st_reverse) {
                        target_epa = &g_active_config.st_epa_left;
                        dir_name = "SOL (Reverse ile)";
                    } else {
                        target_epa = &g_active_config.st_epa_right;
                        dir_name = "SAG";
                    }
                }

                if (target_epa != NULL) {
                    if (inc && *target_epa < 100) *target_epa += 5;
                    if (dec && *target_epa > 30)  *target_epa -= 5;
                    ESP_LOGI(TAG, "ST EPA [%s]: %d%% (Reverse: %s)", 
                             dir_name, *target_epa, g_active_config.st_reverse ? "AKTIF" : "KAPALI");
                } 
                else {
                    // Direksiyon Merkezdeyken: Her İki EPA Aynı Anda (Dual Rate)
                    if (inc) {
                        if (g_active_config.st_epa_left < 100)  g_active_config.st_epa_left += 5;
                        if (g_active_config.st_epa_right < 100) g_active_config.st_epa_right += 5;
                    }
                    if (dec) {
                        if (g_active_config.st_epa_left > 30)  g_active_config.st_epa_left -= 5;
                        if (g_active_config.st_epa_right > 30) g_active_config.st_epa_right -= 5;
                    }
                    ESP_LOGI(TAG, "ST EPA [DUAL RATE]: Sol=%d%% | Sag=%d%%", 
                             g_active_config.st_epa_left, g_active_config.st_epa_right);
                }
                break;
            }

            // --- MENÜ 2: DİREKSİYON EĞRİSİ (CURVE / EXPO) ---
            case CFG_MENU_ST_CURVE:
                if (inc && g_active_config.st_curve < 2) g_active_config.st_curve++;
                if (dec && g_active_config.st_curve > 0) g_active_config.st_curve--;
                ESP_LOGI(TAG, "ST CURVE: %s (%d)", 
                         g_active_config.st_curve == 0 ? "Lineer" : 
                         (g_active_config.st_curve == 1 ? "Expo Yumusak" : "Expo Agresif"), 
                         g_active_config.st_curve);
                break;

            // --- MENÜ 3: GAZ / FREN EPA ---
            case CFG_MENU_TH_EPA: {
                uint8_t *target_th_epa = NULL;
                const char *th_dir_name = NULL;

                if (telemetry->brake > 0.2f) {
                    // Frene Basılı:
                    if (g_active_config.th_reverse) {
                        target_th_epa = &g_active_config.th_epa_forward;
                        th_dir_name = "ILERI (Reverse ile)";
                    } else {
                        target_th_epa = &g_active_config.th_epa_backward;
                        th_dir_name = "GERI/FREN";
                    }
                } else {
                    // Gaz Pedalına Basılı veya Boşta:
                    if (g_active_config.th_reverse) {
                        target_th_epa = &g_active_config.th_epa_backward;
                        th_dir_name = "GERI/FREN (Reverse ile)";
                    } else {
                        target_th_epa = &g_active_config.th_epa_forward;
                        th_dir_name = "ILERI";
                    }
                }

                if (inc && *target_th_epa < 100) *target_th_epa += 5;
                if (dec && *target_th_epa > 20)  *target_th_epa -= 5;
                ESP_LOGI(TAG, "TH EPA [%s]: %d%% (Reverse: %s)", 
                         th_dir_name, *target_th_epa, g_active_config.th_reverse ? "AKTIF" : "KAPALI");
                break;
            }

            // --- MENÜ 4: GAZ EĞRİSİ (CURVE / EXPO) ---
            case CFG_MENU_TH_CURVE:
                if (inc && g_active_config.th_curve < 2) g_active_config.th_curve++;
                if (dec && g_active_config.th_curve > 0) g_active_config.th_curve--;
                ESP_LOGI(TAG, "TH CURVE: %s (%d)", 
                         g_active_config.th_curve == 0 ? "Lineer" : 
                         (g_active_config.th_curve == 1 ? "Expo Yumusak" : "Expo Agresif"), 
                         g_active_config.th_curve);
                break;

            // --- MENÜ 5: DİREKSİYON SUB-TRIM ---
            case CFG_MENU_ST_TRIM:
                if (inc && g_active_config.st_sub_trim < 50)  g_active_config.st_sub_trim += 5;
                if (dec && g_active_config.st_sub_trim > -50) g_active_config.st_sub_trim -= 5;
                ESP_LOGI(TAG, "ST SUB-TRIM: %d (PWM Etkisi: %d us)", 
                         g_active_config.st_sub_trim, g_active_config.st_sub_trim * 3);
                break;

            // --- MENÜ 6: GYRO GAIN (%0 - %100, GPIO 0 PWM) ---
            case CFG_MENU_GYRO_GAIN:
                if (inc && g_active_config.st_gyro_gain <= 95) g_active_config.st_gyro_gain += 2;
                if (dec && g_active_config.st_gyro_gain >= 2)  g_active_config.st_gyro_gain -= 2;
                ESP_LOGI(TAG, "GYRO GAIN: %d%% (PWM: %d us)", 
                         g_active_config.st_gyro_gain, 1000 + (g_active_config.st_gyro_gain * 10));
                break;

            default:
                break;
        }
    }

    // ==============================================================
    // 4. TERS YÖN (REVERSE) GEÇİŞİ (ÜÇGEN TUŞU)
    // ==============================================================
    if (pressed & BTN_TRIANGLE) {
        if (s_current_menu == CFG_MENU_ST_EPA || s_current_menu == CFG_MENU_ST_CURVE) {
            g_active_config.st_reverse = !g_active_config.st_reverse;
            ESP_LOGW(TAG, "ST REVERSE Degistirildi -> %s", g_active_config.st_reverse ? "TERS" : "NORMAL");
            config_changed = true;
        } else if (s_current_menu == CFG_MENU_TH_EPA || s_current_menu == CFG_MENU_TH_CURVE) {
            g_active_config.th_reverse = !g_active_config.th_reverse;
            ESP_LOGW(TAG, "TH REVERSE Degistirildi -> %s", g_active_config.th_reverse ? "TERS" : "NORMAL");
            config_changed = true;
        }
    }

    // ==============================================================
    // 5. ONAY / SIFIRLAMA (X veya ENTER Tuşu)
    //    Tek Tık: Değeri Göster | 1.2 sn Basılı Tutma: Varsayılana Sıfırla
    // ==============================================================
    if (current_buttons & (BTN_ENTER | BTN_CROSS)) {
        if (!s_enter_latched) {
            s_enter_hold_ticks++;
            if (s_enter_hold_ticks >= 60) { // 1.2 saniye
                // Aktif Menüyü Varsayılana Sıfırla
                switch (s_current_menu) {
                    case CFG_MENU_ST_EPA:
                        g_active_config.st_epa_left = 100;
                        g_active_config.st_epa_right = 100;
                        break;
                    case CFG_MENU_ST_CURVE:
                        g_active_config.st_curve = 0;
                        break;
                    case CFG_MENU_TH_EPA:
                        g_active_config.th_epa_forward = 100;
                        g_active_config.th_epa_backward = 100;
                        break;
                    case CFG_MENU_TH_CURVE:
                        g_active_config.th_curve = 0;
                        break;
                    case CFG_MENU_ST_TRIM:
                        g_active_config.st_sub_trim = 0;
                        break;
                    case CFG_MENU_GYRO_GAIN:
                        g_active_config.st_gyro_gain = 50;
                        break;
                    default:
                        break;
                }
                ESP_LOGW(TAG, "Secili Menu Varsayilana Sifirlandi!");
                g29_led_ui_set_raw(G29_LED_ALL);
                vTaskDelay(pdMS_TO_TICKS(150));
                g29_led_ui_clear();
                s_value_display_until = xTaskGetTickCount() + pdMS_TO_TICKS(1500);
                config_changed = true;
                s_enter_latched = true;
            }
        }
    } else {
        if (s_enter_hold_ticks > 0 && !s_enter_latched) {
            // Kısa basıp bırakma: Değer çubuğunu 1.8 sn göster
            s_value_display_until = xTaskGetTickCount() + pdMS_TO_TICKS(1800);
        }
        s_enter_hold_ticks = 0;
        s_enter_latched = false;
    }

    // LED Göstergesini Güncelle
    update_led_display(telemetry);

    // Ayar Değiştiyse Paketi Kopyala ve Dirty Bayrağını Kaldır
    if (config_changed) {
        s_config_dirty = true;
        s_last_config_change_tick = xTaskGetTickCount();

        if (out_config_packet) {
            g_active_config.packet_type = PKT_TYPE_CONFIG;
            *out_config_packet = g_active_config;
        }
    }

    // 5 saniyelik hareketsizlik sonrası otomatik NVS kaydı (Dev Mode'dan çıkılmadan kapatılma güvencesi)
    if (s_config_dirty && ((xTaskGetTickCount() - s_last_config_change_tick) > pdMS_TO_TICKS(5000))) {
        ESP_LOGI(TAG, "5 sn hareketsizlik sonrasi ayarlar guvenle NVS'e kaydediliyor...");
        config_control_save_to_nvs();
        send_save_cmd_to_car();
        s_config_dirty = false;
    }

    return config_changed;
}

