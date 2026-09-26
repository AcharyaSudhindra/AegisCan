#include "uds_server.h"
#include "can_tp.h"
#include "esp_log.h"
#include <stddef.h>

// Minimal truthful diagnostic endpoint. Security access and data-ID services
// remain unsupported until a provisioned authentication/configuration backend exists.
void uds_process_request(const uint8_t *data, uint16_t len) {
    if (!data || len == 0) return;
    uint8_t response[3] = {0x7f, data[0], UDS_NRC_SERVICE_NOT_SUPPORTED};
    uint16_t response_len = 3;
    if (data[0] == UDS_SID_TESTER_PRESENT) {
        if (len != 2) response[2] = 0x13; // Incorrect length/format
        else if ((data[1] & 0x7f) != 0) response[2] = UDS_NRC_SUBFUNCTION_NOT_SUPPORTED;
        else if (data[1] & 0x80) return; // Suppress positive response
        else {
            response[0] = 0x7e;
            response[1] = 0;
            response_len = 2;
        }
    }
    if (cantp_send_response(response, response_len) != ESP_OK)
        ESP_LOGW("UDS", "Diagnostic response could not be queued");
}
