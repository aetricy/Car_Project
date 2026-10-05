#ifndef S3_STATUS_LED_H
#define S3_STATUS_LED_H

// S3 N16R8 kartlarında RGB LED genelde GPIO 48 pinindedir.
#define S3_RGB_LED_PIN 48 

void init_s3_status_led(void);

#endif // S3_STATUS_LED_H