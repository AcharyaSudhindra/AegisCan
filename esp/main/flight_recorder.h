#ifndef FLIGHT_RECORDER_H
#define FLIGHT_RECORDER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

esp_err_t flight_recorder_init(void);
uint32_t flight_recorder_get_count(void);
bool flight_recorder_ready(void);
uint32_t flight_recorder_errors(void);
void task_flight_recorder(void *pvParameters);

#endif // FLIGHT_RECORDER_H
