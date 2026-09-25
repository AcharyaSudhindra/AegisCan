#include "can_tp.h"
#include "uds_server.h"
#include "tinyml_anomaly.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "CAN_TP";

extern QueueHandle_t can_rx_queue;

static cantp_reassembly_t rx_buffer = {0};
static int64_t last_rx_time = 0;

void cantp_send_response(const uint8_t *data, uint16_t len) {
    if (len <= 7) {
        twai_message_t tx_msg = {
            .identifier = 0x7E8, // Standard response ID
            .extd = 0,
            .data_length_code = len + 1,
        };
        tx_msg.data[0] = (CANTP_SINGLE_FRAME << 4) | (len & 0x0F);
        memcpy(&tx_msg.data[1], data, len);
        twai_transmit(&tx_msg, pdMS_TO_TICKS(100));
    } else {
        // First Frame and Consecutive Frames for transmit (simplified)
        twai_message_t tx_msg = {
            .identifier = 0x7E8,
            .extd = 0,
            .data_length_code = 8,
        };
        tx_msg.data[0] = (CANTP_FIRST_FRAME << 4) | ((len >> 8) & 0x0F);
        tx_msg.data[1] = len & 0xFF;
        memcpy(&tx_msg.data[2], data, 6);
        twai_transmit(&tx_msg, pdMS_TO_TICKS(100));
        
        // Wait for flow control in real impl; here we just dump simplified
    }
}

static void send_flow_control() {
    twai_message_t tx_msg = {
        .identifier = 0x7E8, 
        .extd = 0,
        .data_length_code = 3,
    };
    tx_msg.data[0] = (CANTP_FLOW_CONTROL << 4);
    tx_msg.data[1] = 0;  // Block Size = 0 (send all)
    tx_msg.data[2] = 10; // STmin = 10ms
    twai_transmit(&tx_msg, pdMS_TO_TICKS(100));
}

void task_can_tp(void *pvParameters) {
    ESP_LOGI(TAG, "Starting CAN-TP Task");
    
    // Assumes AEGIS_CAN_TX_PIN and AEGIS_CAN_RX_PIN exist in app_config.h
    #ifndef AEGIS_CAN_TX_PIN
    #define AEGIS_CAN_TX_PIN 4
    #define AEGIS_CAN_RX_PIN 5
    #endif

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(AEGIS_CAN_TX_PIN, AEGIS_CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        twai_start();
        ESP_LOGI(TAG, "TWAI driver installed and started");
    } else {
        ESP_LOGE(TAG, "Failed to install TWAI driver");
    }

    twai_message_t rx_msg;
    
    while (1) {
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(50)) == ESP_OK) {
            // Forward to TinyML anomaly detector sliding window
            if (can_rx_queue != NULL) {
                can_frame_entry_t entry;
                entry.arb_id = rx_msg.identifier;
                entry.dlc = rx_msg.data_length_code;
                memcpy(entry.data, rx_msg.data, (rx_msg.data_length_code <= 8) ? rx_msg.data_length_code : 8);
                entry.timestamp_us = esp_timer_get_time();
                xQueueSend(can_rx_queue, &entry, 0);
            }

            uint8_t pci = rx_msg.data[0];
            uint8_t frame_type = pci >> 4;
            
            switch (frame_type) {
                case CANTP_SINGLE_FRAME: {
                    uint8_t len = pci & 0x0F;
                    if (len > 0 && len <= 7) {
                        uds_process_request(&rx_msg.data[1], len);
                    }
                    break;
                }
                case CANTP_FIRST_FRAME: {
                    uint16_t total_len = ((pci & 0x0F) << 8) | rx_msg.data[1];
                    rx_buffer.total_len = total_len;
                    rx_buffer.received_len = 6;
                    rx_buffer.seq_num = 1;
                    rx_buffer.active = true;
                    memcpy(rx_buffer.data, &rx_msg.data[2], 6);
                    last_rx_time = esp_timer_get_time();
                    send_flow_control();
                    break;
                }
                case CANTP_CONSECUTIVE_FRAME: {
                    if (rx_buffer.active) {
                        uint8_t seq = pci & 0x0F;
                        if (seq == (rx_buffer.seq_num & 0x0F)) {
                            uint16_t copy_len = rx_buffer.total_len - rx_buffer.received_len;
                            if (copy_len > 7) copy_len = 7;
                            memcpy(&rx_buffer.data[rx_buffer.received_len], &rx_msg.data[1], copy_len);
                            rx_buffer.received_len += copy_len;
                            rx_buffer.seq_num++;
                            last_rx_time = esp_timer_get_time();
                            
                            if (rx_buffer.received_len >= rx_buffer.total_len) {
                                rx_buffer.active = false;
                                uds_process_request(rx_buffer.data, rx_buffer.total_len);
                            }
                        }
                    }
                    break;
                }
            }
        }
        
        // Timeout incomplete reassembly after 5 seconds
        if (rx_buffer.active && (esp_timer_get_time() - last_rx_time > 5000000)) {
            ESP_LOGW(TAG, "CAN-TP Reassembly timeout");
            rx_buffer.active = false;
        }
    }
}
