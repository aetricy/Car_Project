#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "usb/usb_host.h"

#include "g29_driver_host.h"


static const char *TAG = "G29_DRIVER_HOST";

static usb_host_client_handle_t client_handle = NULL;
static usb_device_handle_t device_handle = NULL;

static volatile bool g29_ready = false;
static uint8_t ep_in_addr = 0;
static uint8_t ep_out_addr = 0;
static uint16_t ep_in_mps = 64; 

static g29_state_callback_t user_state_cb = NULL;
static g29_input_callback_t user_input_cb = NULL;


// Modüler API Fonksiyonları
bool g29_is_ready(void) {
    return g29_ready;
}

void g29_set_leds(uint8_t val) {
    uint8_t buf[] = { 0xf8, 0x12, val, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    g29_send_ffb_command(buf, sizeof(buf));
}

// --- DIREKSIYON ACISI (RANGE) AYARLAMA ---
void g29_set_range(uint16_t range) {

    // Açı değeri 900'den büyük olamaz (G29 donanımsal limiti)
    if (range > 900) range = 900;
    
    // 16-bitlik range değerini düşük (LSB) ve yüksek (MSB) byte olarak ayırıyoruz
    uint8_t range_lsb = range & 0x00FF;
    uint8_t range_msb = (range & 0xFF00) >> 8;
    
    // G29 Açı Komut Paketi
    uint8_t msg[] = { 0xF8, 0x81, range_lsb, range_msb, 0x00, 0x00, 0x00 };
    
    g29_send_ffb_command(msg, sizeof(msg)); 
    vTaskDelay(pdMS_TO_TICKS(20));
}
void g29_set_constant_force(float force) {
    // force: -1.0 (Tam Sol) ile 1.0 (Tam Sağ) arası
    if (force > 1.0f) force = 1.0f;
    if (force < -1.0f) force = -1.0f;

    // JS kütüphanesindeki matematiksel haritalama
    uint8_t val = (uint8_t)(fabs(force) * 255.0f); 

    uint8_t msg[] = { 0x11, 0x00, val, 0x00, 0x00, 0x00, 0x00 };
    g29_send_ffb_command(msg, sizeof(msg));
}

void g29_set_friction(float friction) {
    // friction: 0.0 (Sürtünme yok) ile 1.0 (Maksimum ağırlık)
    if (friction > 1.0f) friction = 1.0f;
    if (friction < 0.0f) friction = 0.0f;

    // Donanım sadece 0x00 ile 0x07 arasını kabul eder
    uint8_t val = (uint8_t)(friction * 7.0f); 

    // Sol ve sağ dönüş için sürtünme değerleri atanır
    uint8_t msg[] = { 0x21, 0x02, val, 0x00, val, 0x00, 0x00 };
    g29_send_ffb_command(msg, sizeof(msg));
   
   
    vTaskDelay(pdMS_TO_TICKS(10));


}

void g29_disable_autocenter(void) {
    // G29'un kendi merkezleme yayını tamamen iptal eder
    uint8_t msg[] = { 0xF5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    g29_send_ffb_command(msg, sizeof(msg));
    vTaskDelay(pdMS_TO_TICKS(10));
}

void g29_force_off(void) {
    // 0xF3 tüm aktif Force Feedback efektlerini siler
    uint8_t msg[] = { 0xF3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    g29_send_ffb_command(msg, sizeof(msg));
    vTaskDelay(pdMS_TO_TICKS(10));
}


// --- OTOMATIK MERKEZLEME (AUTOCENTER) AYARLAMA ---
void g29_set_autocenter(float strength, float rate) {
    vTaskDelay(pdMS_TO_TICKS(20));

    // Güvenlik: Değerlerin 0.0 ile 1.0 arasında olduğundan emin ol (Clamp)
    if (strength > 1.0f) strength = 1.0f;
    if (strength < 0.0f) strength = 0.0f;
    if (rate > 1.0f) rate = 1.0f;
    if (rate < 0.0f) rate = 0.0f;

    // 1. Aşama: Autocenter modunu başlatma mesajı
    uint8_t msg_init[] = { 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    g29_send_ffb_command(msg_init, sizeof(msg_init));

    // ESP32 USB yığılmasını önlemek ve G29'un komutu işlemesi için minik bir nefes (10ms)
    vTaskDelay(pdMS_TO_TICKS(10));

    // 2. Aşama: Güç ve Hız değerlerini hesapla
    // Yolladığın referans kodda strength 15 ile, rate 255 ile çarpılmış. (+0.5f yuvarlama içindir)
    uint8_t s_val = (uint8_t)(strength * 15.0f + 0.5f);
    uint8_t r_val = (uint8_t)(rate * 255.0f + 0.5f);

    // G29 Autocenter Komut Paketi
    uint8_t msg_set[] = { 0xFE, 0x0D, s_val, s_val, r_val, 0x00, 0x00, 0x00 };
    g29_send_ffb_command(msg_set, sizeof(msg_set));
    vTaskDelay(pdMS_TO_TICKS(20));
}


// Transfer callback'leri
static void transfer_complete_cb(usb_transfer_t *transfer) {
    usb_host_transfer_free(transfer);
}

// Genel ham mesaj gönderme fonksiyonu
void g29_send_ffb_command(const uint8_t *command, size_t len) {
    if (!g29_ready || device_handle == NULL || ep_out_addr == 0) return;

    usb_transfer_t *transfer;
    if (usb_host_transfer_alloc(len, 0, &transfer) != ESP_OK) return;

    memcpy(transfer->data_buffer, command, len);
    transfer->num_bytes = len;
    transfer->device_handle = device_handle;
    transfer->bEndpointAddress = ep_out_addr; 
    transfer->callback = transfer_complete_cb;
    transfer->context = NULL;

    if (usb_host_transfer_submit(transfer) != ESP_OK) {
        usb_host_transfer_free(transfer);
    }
}


// Ham girdi okuma callback'i
static void in_transfer_cb(usb_transfer_t *transfer) {
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        if (user_input_cb) {
            user_input_cb(transfer->data_buffer, transfer->actual_num_bytes);
        }
        
        // Başarılı okumadan sonra tekrar dinlemeye devam et
        if (g29_ready) {
            transfer->num_bytes = ep_in_mps; // <-- EKLENDİ (Her turda boyutu güvenceye al)
            usb_host_transfer_submit(transfer);
        } else {
            usb_host_transfer_free(transfer);
        }
    } else {
        // Eğer transfer başarısız olursa nedenini yazdır (Örn: Timeout, Stall)
        ESP_LOGE(TAG, "IN Transfer Hatası! Durum: %d", transfer->status);
        
        // Sessizce ölmek yerine, bağlantı kopmadıysa biraz bekleyip tekrar deniyoruz!
        if (g29_ready) {
            vTaskDelay(pdMS_TO_TICKS(50)); // 50ms nefes al
            usb_host_transfer_submit(transfer); // Tekrar kancayı at
        } else {
            usb_host_transfer_free(transfer);
        }
    }
}

// Arka plan veri okuma görevi
static void g29_input_read_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(500)); 
    
    if (ep_in_addr == 0) {
        ESP_LOGE(TAG, "HATA: IN Endpoint bulunamadı!");
        vTaskDelete(NULL);
        return;
    }

    // <-- AŞAĞIDAKİ SATIR DEĞİŞTİ (Artık 64 yerine cihazın kendi bildirdiği boyutu kullanıyoruz) -->
    size_t transfer_size = ep_in_mps; 

    ESP_LOGI(TAG, "Veri okuma Başlatıldı.");

    usb_transfer_t *in_transfer = NULL;
    if (usb_host_transfer_alloc(transfer_size, 0, &in_transfer) == ESP_OK) {
        in_transfer->device_handle = device_handle;
        in_transfer->bEndpointAddress = ep_in_addr;
        in_transfer->callback = in_transfer_cb;
        // <-- AŞAĞIDAKİ SATIR DEĞİŞTİ -->
        in_transfer->num_bytes = transfer_size; 
        in_transfer->context = NULL;
        
        esp_err_t err = usb_host_transfer_submit(in_transfer);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "İlk IN transfer submit edilemedi! Hata: %d", err);
            usb_host_transfer_free(in_transfer);
        }
    } else {
        ESP_LOGE(TAG, "IN transfer için bellek ayrılamadı!");
    }
    
    vTaskDelete(NULL); 
}

// PS3 Modu Uyandırma (Magic Packet) Görevi
static void g29_wake_up_task(void *arg) {
    ESP_LOGI(TAG, "Magic Packet Yollanıyor...");
    vTaskDelay(pdMS_TO_TICKS(500));
    if (usb_host_interface_claim(client_handle, device_handle, 0, 0) == ESP_OK) {
        uint8_t temp_out_ep = 0x01; // Geçici olarak standart endpoint 1 kabul edilir
        uint8_t msg1[] = { 0xF8, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00 };
        uint8_t msg2[] = { 0xF8, 0x09, 0x05, 0x01, 0x01, 0x00, 0x00 };
        
        // Uyandırma transferleri manuel yapılır çünkü sistem henüz READY değil
        usb_transfer_t *t1;
        if (usb_host_transfer_alloc(sizeof(msg1), 0, &t1) == ESP_OK) {
            memcpy(t1->data_buffer, msg1, sizeof(msg1));
            t1->num_bytes = sizeof(msg1);
            t1->device_handle = device_handle;
            t1->bEndpointAddress = temp_out_ep;
            t1->callback = transfer_complete_cb;
            usb_host_transfer_submit(t1);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        
        usb_transfer_t *t2;
        if (usb_host_transfer_alloc(sizeof(msg2), 0, &t2) == ESP_OK) {
            memcpy(t2->data_buffer, msg2, sizeof(msg2));
            t2->num_bytes = sizeof(msg2);
            t2->device_handle = device_handle;
            t2->bEndpointAddress = temp_out_ep;
            t2->callback = transfer_complete_cb;
            usb_host_transfer_submit(t2);
        }
    }
    vTaskDelete(NULL);
}

// Endpoints Bulma Fonksiyonu
static void find_endpoints(void) {
    const usb_config_desc_t *config_desc;
    if (usb_host_get_active_config_descriptor(device_handle, &config_desc) == ESP_OK) {
        const uint8_t *p = (const uint8_t *)config_desc;
        int offset = 0;

        ep_in_addr = 0;
        ep_out_addr = 0;
        ep_in_mps = 64; // Varsayılan değer

        while (offset < config_desc->wTotalLength) {
            uint8_t len = p[offset];
            uint8_t type = p[offset + 1];
            
            if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
                const usb_ep_desc_t *ep = (const usb_ep_desc_t *)&p[offset];
                
                if ((ep->bmAttributes & 0x03) == 0x03) { 
                    
                    if ((ep->bEndpointAddress & 0x80) != 0) {
                        if (ep_in_addr == 0) {
                            ep_in_addr = ep->bEndpointAddress;
                            // <-- AŞAĞIDAKİ SATIR EKLENDİ -->
                            // Endpoint'in desteklediği Maksimum Paket Boyutunu alıyoruz (0x7FF maskesi standarttır)
                            ep_in_mps = ep->wMaxPacketSize & 0x7FF; 
                        }
                    } else {
                        if (ep_out_addr == 0) {
                            ep_out_addr = ep->bEndpointAddress;
                        }
                    }
                }
            }
            offset += len;
        }
        ESP_LOGI(TAG, "Bulunan EP_IN: 0x%02X (MPS: %d) | EP_OUT: 0x%02X", ep_in_addr, ep_in_mps, ep_out_addr);
    }
}

// Cihaz Olay Yöneticisi
static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg) {
    if (event_msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        vTaskDelay(pdMS_TO_TICKS(300));
        if (device_handle) {
            usb_host_interface_release(client_handle, device_handle, 0);
            usb_host_device_close(client_handle, device_handle);
            device_handle = NULL;
        }

        uint16_t current_pid = 0;
        for (int i = 0; i < 4; i++) {
            if (usb_host_device_open(client_handle, event_msg->new_dev.address, &device_handle) == ESP_OK) {
                const usb_device_desc_t *dev_desc;
                if (usb_host_get_device_descriptor(device_handle, &dev_desc) == ESP_OK) {
                    if (dev_desc->idVendor == 0x046D) {
                        current_pid = dev_desc->idProduct;
                        break;
                    }
                }
                usb_host_device_close(client_handle, device_handle);
                device_handle = NULL;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        if (device_handle == NULL) return;

        if (current_pid == 0xC294) {
            if (user_state_cb) user_state_cb(G29_STATE_PS3_WAKING_UP);
                xTaskCreatePinnedToCore(g29_wake_up_task, "wake_task", 4096, NULL, 5, NULL, USB_TASK_CORE);
        } else if (current_pid == 0xC24F) {
            if (usb_host_interface_claim(client_handle, device_handle, 0, 0) == ESP_OK) {
                find_endpoints();
                g29_ready = true;
                if (user_state_cb) {
                    ESP_LOGI(TAG, "=============================================");
                    ESP_LOGI(TAG, "Başlatılıyor....");
                    vTaskDelay(pdMS_TO_TICKS(5000));   
                    user_state_cb(G29_STATE_NATIVE_READY);
                }
                    xTaskCreatePinnedToCore(g29_input_read_task, "read_task", 4096, NULL, 5, NULL, USB_TASK_CORE);            
                }
        }
    } else if (event_msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        g29_ready = false;
        ep_in_addr = 0;
        ep_out_addr = 0;
        if (user_state_cb) user_state_cb(G29_STATE_DISCONNECTED);
        if (device_handle) {
            usb_host_interface_release(client_handle, device_handle, 0);
            usb_host_device_close(client_handle, device_handle);
            device_handle = NULL;
        }
    }
}

static void usb_lib_task(void *arg) {
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

static void client_task(void *arg) {
    while (1) {
        usb_host_client_handle_events(client_handle, portMAX_DELAY);
    }
}

bool g29_init(g29_state_callback_t state_cb, g29_input_callback_t input_cb) {
    user_state_cb = state_cb;
    user_input_cb = input_cb;

    const usb_host_config_t host_config = { .skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1 };
    if (usb_host_install(&host_config) != ESP_OK) return false;

    xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL, USB_TASK_CORE);

    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async.client_event_callback = client_event_cb,
        .async.callback_arg = NULL,
    };
    if (usb_host_client_register(&client_config, &client_handle) != ESP_OK) return false;

    xTaskCreatePinnedToCore(client_task, "usb_client", 4096, NULL, 5, NULL, USB_TASK_CORE);
    
    return false;
}