#ifndef LED_INDICATOR_H
#define LED_INDICATOR_H

// On ESP32-C3 Supermini, the on-board LED is typically on GPIO 8.
// If your board variant differs, adjust this pin (e.g. 2 or 9).
#define BUILTIN_LED_PIN 8 

// Initializes LED indicator task
void init_led_indicator(void);

#endif // LED_INDICATOR_H