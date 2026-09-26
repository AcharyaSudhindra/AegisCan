#ifndef SPI_DMA_LISTENER_H
#define SPI_DMA_LISTENER_H

#include <stdint.h>
#include <stdbool.h>

enum { EVENT_SOURCE_FPGA = 1, EVENT_SOURCE_RULE = 2 };

typedef struct {
    uint32_t timestamp_24;    // 24-bit FPGA timestamp
    uint8_t  is_extended;     // 1 = CAN 2.0B
    uint32_t arb_id;          // 29-bit arbitration ID
    uint8_t  error_code;      // 4-bit error code
    uint8_t  source;          // FPGA kill report or software rule observation
    int64_t  esp_timestamp_us; // Local ESP32 microsecond timestamp
} violation_descriptor_t;

void task_spi_dma_listener(void *pvParameters);
bool spi_listener_ready(void);
uint32_t spi_listener_rejected(void);
uint32_t spi_listener_dropped(void);

#endif // SPI_DMA_LISTENER_H
