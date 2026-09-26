#include "spi_dma_listener.h"
#include "event_codec.h"
#include "app_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdatomic.h>
#include <string.h>

static const char *TAG = "SPI_DMA_LISTENER";
static atomic_bool initialized;
static atomic_uint rejected, dropped;
bool spi_listener_ready(void) { return atomic_load(&initialized); }
uint32_t spi_listener_rejected(void) { return atomic_load(&rejected); }
uint32_t spi_listener_dropped(void) { return atomic_load(&dropped); }

void task_spi_dma_listener(void *unused) {
    spi_bus_config_t bus = {
        .mosi_io_num = AEGIS_SPI_MOSI_PIN, .miso_io_num = -1,
        .sclk_io_num = AEGIS_SPI_CLK_PIN, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = DESCRIPTOR_SIZE,
    };
    spi_slave_interface_config_t cfg = {
        .mode = 0, .spics_io_num = AEGIS_SPI_CS_PIN, .queue_size = 4,
    };
    gpio_set_pull_mode(AEGIS_SPI_CS_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(AEGIS_SPI_CLK_PIN, GPIO_PULLDOWN_ONLY);
    gpio_set_pull_mode(AEGIS_SPI_MOSI_PIN, GPIO_PULLDOWN_ONLY);
    esp_err_t ret = spi_slave_initialize(SPI_HOST_ID, &bus, &cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI initialization failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    // Four independently owned buffers reduce the gap between transactions.
    // There is still no hardware ready/acknowledgement line to the FPGA.
    uint8_t *buffers = heap_caps_calloc(4, DESCRIPTOR_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    spi_slave_transaction_t transactions[4] = {0};
    if (!buffers) {
        spi_slave_free(SPI_HOST_ID);
        vTaskDelete(NULL);
        return;
    }
    for (unsigned i = 0; i < 4; ++i) {
        transactions[i].length = 64;
        transactions[i].rx_buffer = buffers + i * DESCRIPTOR_SIZE;
        ret = spi_slave_queue_trans(SPI_HOST_ID, &transactions[i], portMAX_DELAY);
        if (ret != ESP_OK) goto stopped;
    }
    atomic_store(&initialized, true);
    while (1) {
        spi_slave_transaction_t *done = NULL;
        ret = spi_slave_get_trans_result(SPI_HOST_ID, &done, portMAX_DELAY);
        if (ret != ESP_OK) goto stopped;
        if (!done) { ret = ESP_FAIL; goto stopped; }
        uint8_t raw[DESCRIPTOR_SIZE];
        memcpy(raw, done->rx_buffer, sizeof(raw));
        size_t received_bits = done->trans_len;
        int64_t now = esp_timer_get_time();
        memset(done->rx_buffer, 0, DESCRIPTOR_SIZE);
        ret = spi_slave_queue_trans(SPI_HOST_ID, done, portMAX_DELAY);
        if (ret != ESP_OK) goto stopped;
        violation_descriptor_t event;
        if (!decode_fpga_event(raw, received_bits, now, &event)) {
            atomic_fetch_add(&rejected, 1);
            continue;
        }
        ESP_LOGI(TAG, "FPGA kill report: ID=0x%lx", (unsigned long)event.arb_id);
        if (xQueueSend(violation_queue, &event, 0) != pdTRUE)
            atomic_fetch_add(&dropped, 1);
        if (xQueueSend(violation_queue_ws, &event, 0) != pdTRUE)
            atomic_fetch_add(&dropped, 1);
    }
stopped:
    atomic_store(&initialized, false);
    ESP_LOGE(TAG, "SPI receiver stopped: %s", esp_err_to_name(ret));
    // Keep queued transaction descriptors/buffers alive while the driver owns them.
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
