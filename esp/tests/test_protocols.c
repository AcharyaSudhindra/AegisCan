#include <assert.h>
#include <stdio.h>
#include "event_codec.h"
#include "cantp_parser.h"

static void check_event(uint32_t id, unsigned extended) {
    uint64_t packed = ((uint64_t)0x123456 << 40) | ((uint64_t)extended << 39) |
                      ((uint64_t)id << 10) | 0x40;
    uint8_t wire[8], plain[64];
    for (int i=0; i<8; ++i) wire[i] = packed >> (56-8*i);
    violation_descriptor_t event = {0};
    assert(decode_fpga_event(wire, 64, 123456789, &event));
    assert(event.arb_id == id && event.is_extended == extended);
    assert(event.timestamp_24 == 0x123456 && event.error_code == 1);
    assert(event.source == EVENT_SOURCE_FPGA && event.esp_timestamp_us == 123456789);
    encode_event_plaintext(&event, plain);
    assert(plain[0] == 1 && plain[1] == EVENT_SOURCE_FPGA);
    assert(plain[8] == (id >> 24) && plain[11] == (id & 255));
    for (unsigned bits=0; bits<64; ++bits) assert(!decode_fpga_event(wire,bits,0,&event));
    wire[7] |= 1; assert(!decode_fpga_event(wire,64,0,&event));
}
int main(void) {
    check_event(0x100,0); check_event(0x001,0); check_event(0x010,0);
    check_event(0x0deadbee,1);
    uint8_t zero[8] = {0}, ones[8]; memset(ones,255,8);
    violation_descriptor_t event;
    assert(!decode_fpga_event(zero,64,0,&event));
    assert(!decode_fpga_event(ones,64,0,&event));
    uint8_t invalid_std[8] = {0,0,0,0,0,0x80,0,0x40};
    assert(!decode_fpga_event(invalid_std,64,0,&event)); // ID 0x2000
    cantp_parser_t p = {0};
    uint8_t sf[8] = {2,0x3e,0};
    assert(cantp_parse(&p,sf,3,0)==TP_COMPLETE && p.total==2 && p.data[0]==0x3e);
    assert(cantp_parse(&p,sf,2,0)==TP_INVALID);
    assert(cantp_parse(&p,sf,0,0)==TP_INVALID);
    uint8_t ff[8] = {0x10,10,0x27,2,1,2,3,4};
    uint8_t cf[8] = {0x21,5,6,7,8};
    assert(cantp_parse(&p,ff,8,0)==TP_FLOW_CONTINUE);
    assert(cantp_parse(&p,cf,5,100)==TP_COMPLETE && p.used==10 && p.data[9]==8);
    assert(cantp_parse(&p,ff,8,0)==TP_FLOW_CONTINUE);
    cf[0]=0x22;
    assert(cantp_parse(&p,cf,5,100)==TP_INVALID && !p.active);
    cf[0]=0x21;
    assert(cantp_parse(&p,ff,8,0)==TP_FLOW_CONTINUE);
    assert(cantp_parse(&p,cf,5,6000000)==TP_IGNORE && !p.active);
    ff[1]=65; assert(cantp_parse(&p,ff,8,0)==TP_FLOW_OVERFLOW);
    ff[1]=3; assert(cantp_parse(&p,ff,8,0)==TP_INVALID);
    ff[1]=10; assert(cantp_parse(&p,ff,7,0)==TP_INVALID);
    // Fuzz malformed lengths/sequences under ASan/UBSan.
    uint32_t rng = 7;
    for (unsigned n=0; n<100000; ++n) {
        uint8_t bytes[8];
        for (unsigned j=0; j<8; ++j) { rng=rng*1664525u+1013904223u; bytes[j]=rng>>24; }
        cantp_parse(&p,bytes,n%10,n*100);
        assert(!p.active || (p.used <= p.total && p.total <= CANTP_MAX_REQUEST));
    }
    puts("PASS: FPGA decode/validation, event encoding, CAN-TP reassembly and malformed-input fuzz");
}
