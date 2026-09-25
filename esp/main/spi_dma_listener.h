#ifndef SPI_DMA_LISTENER_H
#define SPI_DMA_LISTENER_H

#include <stdint.h>

typedef struct {
    uint32_t timestamp_24;    // 24-bit FPGA timestamp
    uint8_t  is_extended;     // 1 = CAN 2.0B
    uint32_t arb_id;          // 29-bit arbitration ID
    uint8_t  error_code;      // 4-bit error code
    int64_t  esp_timestamp_us; // Local ESP32 microsecond timestamp
} violation_descriptor_t;

void task_spi_dma_listener(void *pvParameters);

#endif // SPI_DMA_LISTENER_H
