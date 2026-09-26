#ifndef EVENT_CODEC_H
#define EVENT_CODEC_H
#include "spi_dma_listener.h"
#include <stddef.h>
#include <string.h>

// Accept only complete BLOCKED_ID descriptors emitted by violation_capture.v.
static inline bool decode_fpga_event(const uint8_t *raw, size_t bits,
                                     int64_t now, violation_descriptor_t *out) {
    if (!raw || !out || bits != 64 || (raw[7] & 0x3f) != 0) return false;
    violation_descriptor_t value = {0};
    value.timestamp_24 = ((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2];
    value.is_extended = raw[3] >> 7;
    value.arb_id = ((uint32_t)(raw[3] & 0x7f) << 22) |
                   ((uint32_t)raw[4] << 14) | ((uint32_t)raw[5] << 6) | (raw[6] >> 2);
    value.error_code = ((raw[6] & 3) << 2) | (raw[7] >> 6);
    if (value.error_code != 1 || (!value.is_extended && value.arb_id > 0x7ff)) return false;
    value.source = EVENT_SOURCE_FPGA;
    value.esp_timestamp_us = now;
    *out = value;
    return true;
}

// Versioned plaintext, independent of C padding/endianness; encrypt all 64 bytes.
static inline void encode_event_plaintext(const violation_descriptor_t *event, uint8_t out[64]) {
    memset(out, 0, 64);
    out[0] = 1; // Encoding version
    out[1] = event->source;
    out[2] = event->is_extended;
    out[3] = event->error_code;
    for (unsigned i = 0; i < 4; ++i) {
        out[4+i] = (uint8_t)(event->timestamp_24 >> (24-8*i));
        out[8+i] = (uint8_t)(event->arb_id >> (24-8*i));
    }
    for (unsigned i = 0; i < 8; ++i)
        out[12+i] = (uint8_t)((uint64_t)event->esp_timestamp_us >> (56-8*i));
}
#endif
