#ifndef G29_PROCESSOR_H
#define G29_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#include "g29_config.h"

// ESP-NOW ile C3'e doğrudan yollanacak normalize edilmiş veri paketi


void g29_process_raw_data(const uint8_t *raw_data, int len, g29_telemetry_t *out_telemetry);
void g29_apply_drift_assist(g29_telemetry_t *telemetry, float gyro_yaw_rate);

#endif // G29_PROCESSOR_H