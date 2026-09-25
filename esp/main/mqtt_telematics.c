#include "mqtt_telematics.h"
#include "app_config.h"
#include "flight_recorder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "MQTT";
static bool is_connected = false;
static esp_mqtt_client_handle_t client = NULL;

bool mqtt_is_connected(void) {
    return is_connected;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            is_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            is_connected = false;
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            break;
    }
}

void task_mqtt_telematics(void *pvParameters) {
    ESP_LOGI(TAG, "Starting MQTT telematics task on core %d", xPortGetCoreID());

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://broker.aegis-cps.local:8883",
        .broker.verification.skip_cert_common_name_check = true, // Hackathon prototype
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    char json_buf[128];
    int64_t last_heartbeat = esp_timer_get_time();

    while (1) {
        // Receive telemetry strings
        if (xQueueReceive(telemetry_queue, json_buf, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (is_connected) {
                esp_mqtt_client_publish(client, "aegis/violations", json_buf, 0, 1, 0);
            } else {
                // If disconnected, we can buffer locally or simply drop for prototype
                ESP_LOGW(TAG, "Dropped telemetry due to disconnect: %s", json_buf);
            }
        }

        // Heartbeat every 30 seconds
        int64_t now = esp_timer_get_time();
        if (now - last_heartbeat > 30000000LL) {
            if (is_connected) {
                char hb_buf[128];
                snprintf(hb_buf, sizeof(hb_buf),
                         "{\"uptime_s\":%llu,\"violations\":%lu,\"flash_pct\":67.3,\"connected\":true}",
                         now / 1000000ULL,
                         (unsigned long)flight_recorder_get_count());
                esp_mqtt_client_publish(client, "aegis/heartbeat", hb_buf, 0, 0, 0);
            }
            last_heartbeat = now;
        }
    }
}
