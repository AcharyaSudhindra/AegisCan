#ifndef MQTT_TELEMATICS_H
#define MQTT_TELEMATICS_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void task_mqtt_telematics(void *pvParameters);
bool mqtt_is_connected(void);

#endif // MQTT_TELEMATICS_H
