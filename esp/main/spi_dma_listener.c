#include "spi_dma_listener.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "SPI_DMA_LISTENER";

extern QueueHandle_t violation_queue;
extern QueueHandle_t violation_queue_ws;

void task_spi_dma_listener(void *pvParameters) {
    ESP_LOGI(TAG, "Initializing Hardware SPI Slave DMA listener on Core %d", xPortGetCoreID());

    // Configure SPI Slave Bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = AEGIS_SPI_MOSI_PIN,
        .miso_io_num = AEGIS_SPI_MISO_PIN,
        .sclk_io_num = AEGIS_SPI_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_slave_interface_config_t slvcfg = {
        .mode = 0, // SPI Mode 0 (matches FPGA spi_dma_tx.v)
        .spics_io_num = AEGIS_SPI_CS_PIN,
        .queue_size = 4,
        .flags = 0,
    };

    // Configure pin pulls so floating lines don't trigger false DMA packets
    gpio_set_pull_mode(AEGIS_SPI_CS_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(AEGIS_SPI_CLK_PIN, GPIO_PULLDOWN_ONLY);
    gpio_set_pull_mode(AEGIS_SPI_MOSI_PIN, GPIO_PULLDOWN_ONLY);

    // Enable DMA channel
    esp_err_t ret = spi_slave_initialize(SPI_HOST_ID, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI Slave: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    // Allocate 32-bit word aligned DMA buffer in internal SRAM
    uint8_t *recv_buf = (uint8_t *)heap_caps_malloc(DESCRIPTOR_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!recv_buf) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        vTaskDelete(NULL);
        return;
    }

    spi_slave_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.length    = 64; // 64 bits = 8 bytes
    trans.rx_buffer = recv_buf;

    // PRE-ARM DMA: Queue transaction BEFORE any FPGA transmission occurs.
    // This eliminates the 320 ns race condition completely.
    ESP_ERROR_CHECK(spi_slave_queue_trans(SPI_HOST_ID, &trans, portMAX_DELAY));
    ESP_LOGI(TAG, "SPI Slave DMA Armed & Ready. Listening for FPGA transmissions...");

    while (1) {
        spi_slave_transaction_t *ret_trans = NULL;

        // Block until FPGA drives CS_N low, clocks 64 bits, and releases CS_N
        ret = spi_slave_get_trans_result(SPI_HOST_ID, &ret_trans, portMAX_DELAY);
        if (ret == ESP_OK && ret_trans != NULL) {

            // Copy received bytes immediately before re-arming the buffer
            uint8_t raw[DESCRIPTOR_SIZE];
            memcpy(raw, recv_buf, DESCRIPTOR_SIZE);

            // Re-arm DMA immediately for the next packet (pre-arm pattern)
            trans.length    = 64;
            trans.rx_buffer = recv_buf;
            spi_slave_queue_trans(SPI_HOST_ID, &trans, portMAX_DELAY);

            // Parse 64-bit hardware violation descriptor from raw[] copy
            violation_descriptor_t desc;

            // Bits [63:40]: 24-bit FPGA timestamp
            desc.timestamp_24 = ((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2];

            // Bit [39]: is_extended
            desc.is_extended = (raw[3] & 0x80) ? 1 : 0;

            // Bits [38:10]: 29-bit Arbitration ID
            desc.arb_id = ((uint32_t)(raw[3] & 0x7F) << 22) |
                          ((uint32_t)raw[4] << 14) |
                          ((uint32_t)raw[5] << 6)  |
                          (raw[6] >> 2);

            // Bits [9:6]: 4-bit error code
            desc.error_code = ((raw[6] & 0x03) << 2) | ((raw[7] & 0xC0) >> 6);

            desc.esp_timestamp_us = esp_timer_get_time();

            // Filter floating noise / disconnected lines (all 1s or all 0s)
            if ((desc.arb_id == 0x1FFFFFFF && desc.error_code == 15) ||
                (desc.arb_id == 0 && desc.error_code == 0 && desc.timestamp_24 == 0)) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            ESP_LOGW(TAG, "FPGA KILL: ID=0x%03lX %s | ErrCode=%d | Tick=%lu",
                     (unsigned long)desc.arb_id,
                     desc.is_extended ? "(Ext-29b)" : "(Std-11b)",
                     desc.error_code,
                     (unsigned long)desc.timestamp_24);

            if (violation_queue != NULL) {
                if (xQueueSend(violation_queue, &desc, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "violation_queue full, dropping AES/log event");
                }
            }
            // Separately fan-out to the WebSocket dashboard queue
            if (violation_queue_ws != NULL) {
                if (xQueueSend(violation_queue_ws, &desc, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "violation_queue_ws full, dropping dashboard event");
                }
            }
        }
    }
}
