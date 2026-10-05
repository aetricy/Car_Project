#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

// ESP32-C3 Supermini üzerinde dahili LED genellikle GPIO 8 pinindedir.
// Eğer senin kartında farklıysa bu numarayı (örn: 2 veya 9) değiştirebilirsin.
#define BUILTIN_LED_PIN 8 

// LED Task'ını başlatan fonksiyon
void init_led_indicator(void);

#endif // LED_INDICATOR_H