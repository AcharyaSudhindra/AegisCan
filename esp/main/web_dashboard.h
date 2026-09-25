#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize and start the Aegis-CAN Web Dashboard HTTP & WebSocket Server.
 */
esp_err_t web_dashboard_start(void);

/**
 * @brief Task that reads from violation_queue and broadcasts telemetry to all connected WebSocket clients.
 */
void task_web_dashboard(void *pvParameters);

#endif // WEB_DASHBOARD_H
