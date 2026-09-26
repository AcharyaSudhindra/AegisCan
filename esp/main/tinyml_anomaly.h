#ifndef TINYML_ANOMALY_H
#define TINYML_ANOMALY_H

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

// CAN frame data structure for the sliding window
typedef struct {
    uint32_t arb_id;          // CAN Arbitration ID
    uint8_t  data[8];         // CAN data payload (up to 8 bytes)
    uint8_t  dlc;             // Data Length Code
    bool     is_extended;
    int64_t  timestamp_us;    // ESP32 microsecond timestamp
} can_frame_entry_t;

// Anomaly result
typedef struct {
    float    confidence;       // 0.0 - 1.0 anomaly score
    uint32_t arb_id;          // ID that triggered anomaly
    uint8_t  anomaly_type;    // 0=temporal, 1=value_range, 2=frequency
} anomaly_result_t;

/**
 * @brief Initialize the TinyML anomaly detection engine
 */
void tinyml_anomaly_init(void);

/**
 * @brief Task for TinyML anomaly detection processing
 * 
 * @param pvParameters FreeRTOS task parameters
 */
void task_tinyml_anomaly(void *pvParameters);

#endif // TINYML_ANOMALY_H
