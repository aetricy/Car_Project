#ifndef S3_STATUS_LED_H
#define S3_STATUS_LED_H

// On ESP32-S3 (e.g. N16R8), on-board addressable RGB LED is typically on GPIO 48.
#define S3_RGB_LED_PIN 48 

void init_s3_status_led(void);

#endif // S3_STATUS_LED_H