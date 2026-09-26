#include "can_tp.h"
#include "cantp_parser.h"
#include "uds_server.h"
#include "tinyml_anomaly.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>

static atomic_bool initialized;
bool can_tp_ready(void) {
    twai_status_info_t status;
    return atomic_load(&initialized) && twai_get_status_info(&status) == ESP_OK &&
           status.state == TWAI_STATE_RUNNING;
}
esp_err_t cantp_send_response(const uint8_t *data, uint16_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;
    // Supported diagnostic responses fit a single frame. Never truncate a long response.
    if (len > 7) return ESP_ERR_NOT_SUPPORTED;
    twai_message_t tx = {.identifier = 0x7e8, .data_length_code = len + 1};
    tx.data[0] = len;
    memcpy(tx.data + 1, data, len);
    return twai_transmit(&tx, pdMS_TO_TICKS(100));
}
void task_can_tp(void *unused) {
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t err = twai_driver_install(&general, &timing, &filter);
    if (err != ESP_OK) {
        ESP_LOGE("CAN_TP", "TWAI install failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL); return;
    }
    err = twai_start();
    if (err != ESP_OK) {
        twai_driver_uninstall();
        ESP_LOGE("CAN_TP", "TWAI start failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL); return;
    }
    atomic_store(&initialized, true);
    ESP_LOGI("CAN_TP", "TWAI driver started on TX=%d RX=%d; physical link unverified", CAN_TX_PIN, CAN_RX_PIN);
    cantp_parser_t parser = {0};
    while (1) {
        twai_message_t rx;
        if (twai_receive(&rx, pdMS_TO_TICKS(50)) != ESP_OK) continue;
        if (rx.rtr || rx.data_length_code > 8) continue;
        can_frame_entry_t entry = {0};
        entry.arb_id = rx.identifier;
        entry.is_extended = rx.extd;
        entry.dlc = rx.data_length_code;
        entry.timestamp_us = esp_timer_get_time();
        memcpy(entry.data, rx.data, entry.dlc);
        if (xQueueSend(can_rx_queue, &entry, 0) != pdTRUE)
            ESP_LOGW("CAN_TP", "Anomaly input queue full");
        // Do not interpret normal RPM/speed payloads as diagnostic messages.
        if (rx.extd || rx.identifier != 0x7e0) continue;
        int action = cantp_parse(&parser, rx.data, rx.data_length_code, entry.timestamp_us);
        if (action == TP_COMPLETE) uds_process_request(parser.data, parser.total);
        else if (action == TP_FLOW_CONTINUE || action == TP_FLOW_OVERFLOW) {
            twai_message_t fc = {.identifier = 0x7e8, .data_length_code = 3};
            fc.data[0] = action == TP_FLOW_CONTINUE ? 0x30 : 0x32;
            fc.data[2] = 10;
            if (twai_transmit(&fc, pdMS_TO_TICKS(100)) != ESP_OK) parser.active = false;
        }
    }
}
