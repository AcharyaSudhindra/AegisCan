/**
 * @file tinyml_anomaly.c
 * @brief INT8-quantized TinyML 1D-CNN anomaly detection engine (Placeholder)
 *
 * NOTE: Since TensorFlow Lite Micro requires C++ and a trained model (which we 
 * don't have yet), this implements a rule-based anomaly detector as a placeholder 
 * that mirrors the API the future TinyML model will use. This is practical for 
 * the hackathon. 
 *
 * This will be replaced with actual TFLite Micro inference when the trained 
 * INT8 model is available on the "model" partition.
 */

#include "tinyml_anomaly.h"
#include "app_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "TINYML";

// Number of different CAN Arbitration IDs we track
#define MAX_MONITORED_IDS 32

// Configuration (fallback if not in app_config.h)
#ifndef TINYML_INPUT_WINDOW
#define TINYML_INPUT_WINDOW 16
#endif

#ifndef TINYML_ANOMALY_THRESHOLD
#define TINYML_ANOMALY_THRESHOLD 0.85f
#endif

typedef struct {
    uint32_t arb_id;
    bool active;
    can_frame_entry_t window[TINYML_INPUT_WINDOW];
    uint8_t window_idx;
    uint8_t window_count;
} sliding_window_t;

static sliding_window_t *windows = NULL;

// External queues
extern QueueHandle_t violation_queue;
// Assuming a can_rx_queue exists for incoming frames to the anomaly task
extern QueueHandle_t can_rx_queue; 

void tinyml_anomaly_init(void) {
    ESP_LOGI(TAG, "Initializing TinyML Anomaly Detection (Rule-based Placeholder)");
    
    // Allocate sliding window buffers in PSRAM
    windows = (sliding_window_t *)heap_caps_malloc(sizeof(sliding_window_t) * MAX_MONITORED_IDS, MALLOC_CAP_SPIRAM);
    if (windows == NULL) {
        ESP_LOGE(TAG, "Failed to allocate sliding windows in PSRAM");
        return;
    }
    
    memset(windows, 0, sizeof(sliding_window_t) * MAX_MONITORED_IDS);
    ESP_LOGI(TAG, "TinyML Initialization Complete");
}

static sliding_window_t* get_or_create_window(uint32_t arb_id) {
    if (windows == NULL) return NULL;
    
    // Search for existing
    for (int i = 0; i < MAX_MONITORED_IDS; i++) {
        if (windows[i].active && windows[i].arb_id == arb_id) {
            return &windows[i];
        }
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_MONITORED_IDS; i++) {
        if (!windows[i].active) {
            windows[i].active = true;
            windows[i].arb_id = arb_id;
            windows[i].window_idx = 0;
            windows[i].window_count = 0;
            return &windows[i];
        }
    }
    
    return NULL;
}

static void check_anomalies(sliding_window_t *win) {
    if (win->window_count < 2) return; // Need at least 2 frames for comparison
    
    uint8_t prev_idx = (win->window_idx == 0) ? (TINYML_INPUT_WINDOW - 1) : (win->window_idx - 1);
    can_frame_entry_t *latest = &win->window[prev_idx];
    
    uint8_t prev_prev_idx = (prev_idx == 0) ? (TINYML_INPUT_WINDOW - 1) : (prev_idx - 1);
    can_frame_entry_t *previous = &win->window[prev_prev_idx];
    
    float confidence = 0.0f;
    uint8_t anomaly_type = 0;
    
    int64_t delta_us = latest->timestamp_us - previous->timestamp_us;
    
    // a) Temporal anomaly: delta < 1ms or > 500ms
    if (delta_us < 1000 || delta_us > 500000) {
        confidence = 0.90f;
        anomaly_type = 0; // Temporal
    }
    
    // b) Value range anomaly
    if (confidence < TINYML_ANOMALY_THRESHOLD && latest->dlc > 0 && previous->dlc > 0) {
        // Simple heuristic: compare the first byte. If it jumps by more than 50%
        int diff = abs((int)latest->data[0] - (int)previous->data[0]);
        if (previous->data[0] != 0 && ((float)diff / (float)previous->data[0]) > 0.5f) {
            confidence = 0.88f;
            anomaly_type = 1; // Value range
        }
    }
    
    // c) Frequency anomaly (simplified logic here)
    // We could calculate the average delta of the window to find sudden rate changes
    
    if (confidence > TINYML_ANOMALY_THRESHOLD) {
        ESP_LOGW(TAG, "Anomaly detected! ID: 0x%lx, Type: %d, Confidence: %.2f", win->arb_id, anomaly_type, confidence);
        
        if (violation_queue != NULL) {
            uint64_t violation_desc = 0;
            
            // 64-bit Violation Descriptor Format:
            // [63:40] = timestamp[23:0]
            // [39]    = is_extended
            // [38:10] = arb_id[28:0]
            // [9:6]   = error_code[3:0]
            // [5:0]   = reserved
            
            uint32_t ts_24 = (uint32_t)(latest->timestamp_us & 0xFFFFFF);
            uint32_t is_ext = (win->arb_id > 0x7FF) ? 1 : 0;
            uint32_t id_29 = win->arb_id & 0x1FFFFFFF;
            uint8_t err_code = 0x0A; // Using 0x0A as a custom error code for TinyML anomaly
            
            violation_desc |= ((uint64_t)ts_24 << 40);
            violation_desc |= ((uint64_t)is_ext << 39);
            violation_desc |= ((uint64_t)id_29 << 10);
            violation_desc |= ((uint64_t)err_code << 6);
            
            xQueueSend(violation_queue, &violation_desc, 0);
        }
    }
}

void task_tinyml_anomaly(void *pvParameters) {
    ESP_LOGI(TAG, "TinyML Task Started on Core %d", xPortGetCoreID());
    
    can_frame_entry_t frame;
    
    while (1) {
        if (can_rx_queue != NULL) {
            if (xQueueReceive(can_rx_queue, &frame, portMAX_DELAY) == pdTRUE) {
                sliding_window_t *win = get_or_create_window(frame.arb_id);
                if (win) {
                    // Add to sliding window
                    win->window[win->window_idx] = frame;
                    win->window_idx = (win->window_idx + 1) % TINYML_INPUT_WINDOW;
                    if (win->window_count < TINYML_INPUT_WINDOW) {
                        win->window_count++;
                    }
                    
                    // Run anomaly checks on the window
                    check_anomalies(win);
                }
            }
        } else {
            // If the queue isn't initialized yet, delay and check again
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
