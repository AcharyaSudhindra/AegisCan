#ifndef CANTP_PARSER_H
#define CANTP_PARSER_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define CANTP_MAX_REQUEST 64
typedef struct {
    uint8_t data[CANTP_MAX_REQUEST];
    uint16_t total, used;
    uint8_t next_seq;
    bool active;
    int64_t last_us;
} cantp_parser_t;
enum { TP_IGNORE, TP_COMPLETE, TP_FLOW_CONTINUE, TP_FLOW_OVERFLOW, TP_INVALID };
static inline int cantp_parse(cantp_parser_t *p, const uint8_t *data, uint8_t dlc, int64_t now) {
    if (p->active && now - p->last_us > 5000000) p->active = false;
    if (!data || dlc == 0 || dlc > 8) { p->active = false; return TP_INVALID; }
    unsigned type = data[0] >> 4;
    if (type == 0) {
        p->active = false;
        unsigned len = data[0] & 15;
        if (len == 0 || len > 7 || len + 1 > dlc) return TP_INVALID;
        memcpy(p->data, data + 1, len);
        p->total = p->used = len;
        return TP_COMPLETE;
    }
    if (type == 1) {
        p->active = false;
        if (dlc != 8) return TP_INVALID;
        unsigned len = ((data[0] & 15) << 8) | data[1];
        if (len <= 7) return TP_INVALID;
        if (len > CANTP_MAX_REQUEST) return TP_FLOW_OVERFLOW;
        memcpy(p->data, data + 2, 6);
        p->total = len; p->used = 6; p->next_seq = 1;
        p->last_us = now; p->active = true;
        return TP_FLOW_CONTINUE;
    }
    if (type == 2 && p->active) {
        unsigned count = p->total - p->used;
        if (count > 7) count = 7;
        if ((data[0] & 15) != p->next_seq || dlc < count + 1) {
            p->active = false;
            return TP_INVALID;
        }
        memcpy(p->data + p->used, data + 1, count);
        p->used += count;
        p->next_seq = (p->next_seq + 1) & 15;
        p->last_us = now;
        if (p->used == p->total) { p->active = false; return TP_COMPLETE; }
    }
    return TP_IGNORE;
}
#endif
