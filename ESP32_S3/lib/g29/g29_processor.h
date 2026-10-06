#ifndef G29_PROCESSOR_H
#define G29_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#include "g29_config.h"
#include "g29_button_map.h"




void g29_process_raw_data(const uint8_t *raw_data, int len, g29_telemetry_t *out_telemetry);
void g29_create_drive_packet(const g29_telemetry_t *telemetry, car_drive_packet_t *out_packet);

#endif // G29_PROCESSOR_H