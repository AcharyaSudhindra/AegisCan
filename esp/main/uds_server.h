#ifndef UDS_SERVER_H
#define UDS_SERVER_H

#include <stdint.h>

#define UDS_SID_DIAGNOSTIC_SESSION    0x10
#define UDS_SID_SECURITY_ACCESS       0x27
#define UDS_SID_WRITE_DATA_BY_ID      0x2E
#define UDS_SID_TESTER_PRESENT        0x3E
#define UDS_SID_READ_DATA_BY_ID       0x22

#define UDS_NRC_SERVICE_NOT_SUPPORTED        0x11
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED     0x12
#define UDS_NRC_SECURITY_ACCESS_DENIED        0x33
#define UDS_NRC_INVALID_KEY                   0x35
#define UDS_NRC_EXCEEDED_ATTEMPTS             0x36
#define UDS_NRC_REQUIRED_TIME_DELAY           0x37

void task_uds_server(void *pvParameters);
void uds_process_request(const uint8_t *data, uint16_t len);

#endif // UDS_SERVER_H
