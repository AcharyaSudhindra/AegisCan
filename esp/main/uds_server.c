#include "uds_server.h"
#include "can_tp.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "UDS_SERVER";

typedef enum {
    UDS_STATE_LOCKED,
    UDS_STATE_UNLOCKED
} uds_state_t;

static uds_state_t security_state = UDS_STATE_LOCKED;
static uint8_t current_seed[8];
static const uint8_t secret_key[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}; // UDS 8-byte key
static int failed_attempts = 0;
static int64_t lock_timer = 0;

static QueueHandle_t uds_req_queue;

typedef struct {
    uint8_t data[4096];
    uint16_t len;
} uds_req_t;

void uds_process_request(const uint8_t *data, uint16_t len) {
    uds_req_t req;
    if (len <= sizeof(req.data) && uds_req_queue != NULL) {
        memcpy(req.data, data, len);
        req.len = len;
        xQueueSend(uds_req_queue, &req, 0);
    }
}

static void send_nrc(uint8_t sid, uint8_t nrc) {
    uint8_t resp[3] = {0x7F, sid, nrc};
    cantp_send_response(resp, 3);
}

void task_uds_server(void *pvParameters) {
    ESP_LOGI(TAG, "Starting UDS Server Task");
    uds_req_queue = xQueueCreate(10, sizeof(uds_req_t));
    uds_req_t req;
    
    while (1) {
        if (xQueueReceive(uds_req_queue, &req, portMAX_DELAY)) {
            uint8_t sid = req.data[0];
            
            if (sid == UDS_SID_TESTER_PRESENT) {
                // Respond with positive response
                uint8_t resp[2] = {sid + 0x40, 0x00};
                if (req.len > 1) {
                    resp[1] = req.data[1]; // Sub-function
                }
                cantp_send_response(resp, req.len > 1 ? 2 : 1);
            }
            else if (sid == UDS_SID_SECURITY_ACCESS) {
                if (req.len < 2) {
                    send_nrc(sid, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
                    continue;
                }
                uint8_t sub = req.data[1];
                if (sub == 0x01) { // requestSeed
                    if (lock_timer && (esp_timer_get_time() - lock_timer < 10000000)) {
                        send_nrc(sid, UDS_NRC_REQUIRED_TIME_DELAY);
                        continue;
                    }
                    lock_timer = 0;
                    esp_fill_random(current_seed, sizeof(current_seed));
                    uint8_t resp[10] = {sid + 0x40, sub};
                    memcpy(&resp[2], current_seed, 8);
                    cantp_send_response(resp, 10);
                } else if (sub == 0x02) { // sendKey
                    if (lock_timer && (esp_timer_get_time() - lock_timer < 10000000)) {
                        send_nrc(sid, UDS_NRC_REQUIRED_TIME_DELAY);
                        continue;
                    }
                    if (req.len < 10) {
                        send_nrc(sid, UDS_NRC_INVALID_KEY);
                        continue;
                    }
                    uint8_t expected_key[8];
                    for (int i = 0; i < 8; i++) expected_key[i] = current_seed[i] ^ secret_key[i];
                    
                    if (memcmp(&req.data[2], expected_key, 8) == 0) {
                        security_state = UDS_STATE_UNLOCKED;
                        failed_attempts = 0;
                        uint8_t resp[2] = {sid + 0x40, sub};
                        cantp_send_response(resp, 2);
                        ESP_LOGI(TAG, "UDS Security Access UNLOCKED");
                    } else {
                        failed_attempts++;
                        if (failed_attempts >= 3) {
                            lock_timer = esp_timer_get_time();
                            send_nrc(sid, UDS_NRC_EXCEEDED_ATTEMPTS);
                            ESP_LOGW(TAG, "UDS Security locked out (3 failed attempts)");
                        } else {
                            send_nrc(sid, UDS_NRC_INVALID_KEY);
                        }
                    }
                } else {
                    send_nrc(sid, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
                }
            }
            else if (sid == UDS_SID_WRITE_DATA_BY_ID || sid == UDS_SID_READ_DATA_BY_ID) {
                if (security_state != UDS_STATE_UNLOCKED) {
                    send_nrc(sid, UDS_NRC_SECURITY_ACCESS_DENIED);
                } else {
                    // Placeholder for actual BRAM writing and violation stats reading
                    uint8_t resp[3] = {sid + 0x40, req.data[1], req.data[2]};
                    cantp_send_response(resp, 3);
                    ESP_LOGI(TAG, "Processed UDS Service %02X", sid);
                }
            }
            else {
                send_nrc(sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
            }
        }
    }
}
