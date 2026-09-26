#ifndef CAN_TP_H
#define CAN_TP_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    CANTP_SINGLE_FRAME = 0,
    CANTP_FIRST_FRAME = 1,
    CANTP_CONSECUTIVE_FRAME = 2,
    CANTP_FLOW_CONTROL = 3
} cantp_frame_type_t;

void task_can_tp(void *pvParameters);
esp_err_t cantp_send_response(const uint8_t *data, uint16_t len);
bool can_tp_ready(void);

#endif // CAN_TP_H
