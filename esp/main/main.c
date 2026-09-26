#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "app_config.h"

// Task headers
#include "spi_dma_listener.h"
#include "can_tp.h"
#include "uds_server.h"
#include "aes_gcm_engine.h"
#include "flight_recorder.h"
#include "mqtt_telematics.h"
#include "tinyml_anomaly.h"
#include "web_dashboard.h"

static const char *TAG = "MAIN";

// Global queues
QueueHandle_t violation_queue;      // → task_aes_gcm_engine (encrypt + log)
QueueHandle_t violation_queue_ws;   // → task_web_dashboard  (WebSocket broadcast)
QueueHandle_t encrypted_queue;
QueueHandle_t telemetry_queue;
QueueHandle_t can_rx_queue;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to your Hotspot 'Sudhindra'...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* dis = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Hotspot dropped (reason code=%d), reconnecting...", dis->reason);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "****************************************************************");
        ESP_LOGI(TAG, "  >>> SUCCESS! CONNECTED TO HOTSPOT 'Sudhindra' <<<");
        ESP_LOGI(TAG, "  >>> OPEN HUD ON LAPTOP AT: http://" IPSTR " <<<", IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "****************************************************************");
    }
}

static void wifi_init_dual(void) {
    ESP_LOGI(TAG, "Initializing Wi-Fi Dual Mode (Hotspot Station + Sentry AP)...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t sta_config = {
        .sta = {
            .ssid = "Sudhindra",
            .password = "sudhindra2024@",
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
        },
    };

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AEGIS_WIFI_SSID,
            .ssid_len = strlen(AEGIS_WIFI_SSID),
            .channel = AEGIS_WIFI_CHANNEL,
            .password = AEGIS_WIFI_PASS,
            .max_connection = AEGIS_MAX_STA_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Dual Wi-Fi Online: Joining 'Sudhindra' + Broadcasting '%s'", AEGIS_WIFI_SSID);
}


void app_main(void) {
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "    Aegis-CAN Tier 2 Gateway Starting    ");
    ESP_LOGI(TAG, "    Target: ESP32-S3 N16R8 Hardware       ");
    ESP_LOGI(TAG, "=========================================");

    // 0. Print N16R8 Hardware Memory Diagnostics
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip: ESP32-S3 (Revision %d, %d Cores)", chip_info.revision, chip_info.cores);
    ESP_LOGI(TAG, "Internal SRAM Free: %u KB", (unsigned int)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_free > 0) {
        ESP_LOGI(TAG, "Octal PSRAM Active: %u KB (~%.2f MB) free", 
                 (unsigned int)(psram_free / 1024), 
                 (float)psram_free / (1024.0f * 1024.0f));
    } else {
        ESP_LOGW(TAG, "Octal PSRAM not detected or not mapped!");
    }

    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize LittleFS Flight Recorder partition
    if (flight_recorder_init() != ESP_OK)
        ESP_LOGE(TAG, "Flight recorder unavailable; logging disabled");

    // 3. Initialize TinyML anomaly engine (allocates sliding windows in PSRAM)
    tinyml_anomaly_init();

    // 4. Initialize Wi-Fi in Dual Mode (Hotspot Client + Standalone Sentry AP)
    wifi_init_dual();

    // 5. Create FreeRTOS Queues
    violation_queue = xQueueCreate(QUEUE_VIOLATION_LEN, sizeof(violation_descriptor_t));
    if (violation_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create violation_queue");
    }

    violation_queue_ws = xQueueCreate(QUEUE_VIOLATION_LEN, sizeof(violation_descriptor_t));
    if (violation_queue_ws == NULL) {
        ESP_LOGE(TAG, "Failed to create violation_queue_ws");
    }

    encrypted_queue = xQueueCreate(QUEUE_ENCRYPTED_LEN, sizeof(encrypted_record_t));
    if (encrypted_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create encrypted_queue");
    }

    telemetry_queue = xQueueCreate(QUEUE_TELEMETRY_LEN, 128); // 128 bytes for JSON string
    if (telemetry_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create telemetry_queue");
    }

    can_rx_queue = xQueueCreate(32, sizeof(can_frame_entry_t));
    if (can_rx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create can_rx_queue");
    }

    // 6. Create Tasks
    ESP_ERROR_CHECK(violation_queue && violation_queue_ws && encrypted_queue &&
                    telemetry_queue && can_rx_queue ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "Spawning tasks...");

    // Core 0: Real-time IO tasks
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task_spi_dma_listener, "spi_dma", STACK_SPI_DMA_LISTENER, NULL, PRIO_SPI_DMA_LISTENER, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task_can_tp, "can_tp", STACK_CAN_TP, NULL, PRIO_CAN_TP, NULL, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    // Core 1: Compute & Telematics tasks
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task_aes_gcm_engine, "aes_gcm", STACK_AES_GCM_ENGINE, NULL, PRIO_AES_GCM_ENGINE, NULL, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task_flight_recorder, "flight_rec", STACK_FLIGHT_RECORDER, NULL, PRIO_FLIGHT_RECORDER, NULL, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task_web_dashboard, "web_dash", STACK_WEB_DASHBOARD, NULL, PRIO_WEB_DASHBOARD, NULL, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task_mqtt_telematics, "mqtt_tel", STACK_MQTT_TELEMATICS, NULL, PRIO_MQTT_TELEMATICS, NULL, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task_tinyml_anomaly, "tinyml", STACK_TINYML_ANOMALY, NULL, PRIO_TINYML_ANOMALY, NULL, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_LOGI(TAG, "All tasks created. Aegis-CAN Gateway is live.");
}
