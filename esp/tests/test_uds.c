#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "can_tp.h"
#include "uds_server.h"
static unsigned calls;
static uint8_t last[8];
static uint16_t last_len;
esp_err_t cantp_send_response(const uint8_t *data, uint16_t len) {
    assert(len <= 7); memcpy(last,data,len); last_len=len; ++calls; return ESP_OK;
}
int main(void) {
    uint8_t tester[] = {0x3e,0};
    uds_process_request(tester,2);
    assert(calls==1 && last_len==2 && last[0]==0x7e && last[1]==0);
    tester[1]=0x80; uds_process_request(tester,2); assert(calls==1);
    tester[1]=1; uds_process_request(tester,2); assert(last[0]==0x7f && last[2]==0x12);
    uds_process_request(tester,1); assert(last[2]==0x13);
    uint8_t unsupported[] = {0x27,2,0,0,0,0,0,0,0,0};
    uds_process_request(unsupported,sizeof(unsupported));
    assert(last[0]==0x7f && last[1]==0x27 && last[2]==0x11);
    unsupported[0]=0x2e; uds_process_request(unsupported,sizeof(unsupported));
    assert(last[0]==0x7f && last[1]==0x2e && last[2]==0x11);
    unsupported[0]=0x22; uds_process_request(unsupported,1); assert(last[2]==0x11);
    unsigned before=calls;
    uds_process_request(NULL,0); assert(calls==before);
    puts("PASS: TesterPresent, suppressed responses, invalid lengths and unsupported UDS services");
}
