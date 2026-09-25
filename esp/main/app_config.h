#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// SPI Definitions
#define AEGIS_SPI_CLK_PIN   12
#define AEGIS_SPI_MOSI_PIN  11
#define AEGIS_SPI_MISO_PIN  13
#define AEGIS_SPI_CS_PIN    10
#define AEGIS_SPI_IRQ_PIN   9

#define SPI_HOST_ID         SPI2_HOST
#define SPI_CLOCK_HZ        12500000 // 12.5 MHz
#define DESCRIPTOR_SIZE     8

// CAN Definitions
#define CAN_TX_PIN          4
#define CAN_RX_PIN          5
#define CAN_BITRATE         500000

// Queue Lengths
#define QUEUE_VIOLATION_LEN 32
#define QUEUE_ENCRYPTED_LEN 16
#define QUEUE_TELEMETRY_LEN 16

// AES Configuration
#define AES_KEY_SIZE_BITS   256
#define AES_IV_SIZE_BYTES   12
#define AES_TAG_SIZE_BYTES  16

// Flight Log
#define FLIGHT_LOG_PARTITION "flight_log"
#define FLIGHT_LOG_MOUNT     "/flight"
#define FLIGHT_LOG_RECORD_SIZE 128

// MQTT Telematics
#define MQTT_BROKER_URI      "mqtts://broker.aegis-cps.local:8883"
#define MQTT_TOPIC_VIOLATION "aegis/violations"
#define MQTT_TOPIC_HEARTBEAT "aegis/heartbeat"

// TinyML Anomaly
#define TINYML_MODEL_PARTITION "model"
#define TINYML_WINDOW_SIZE     16
#define TINYML_THRESHOLD       0.85f

// UDS Server
#define UDS_SEED_KEY_SIZE      8
#define UDS_TIMEOUT_MS         5000

// Wi-Fi SoftAP Configuration (Standalone Judge Demo Network)
#define AEGIS_WIFI_SSID         "Aegis-CAN-Sentry"
#define AEGIS_WIFI_PASS         "" // Open AP for seamless zero-friction demo
#define AEGIS_WIFI_CHANNEL      1
#define AEGIS_MAX_STA_CONN      4

// Task Stack Sizes
#define STACK_SPI_DMA_LISTENER  4096
#define STACK_CAN_TP            4096
#define STACK_UDS_SERVER        4096
#define STACK_AES_GCM_ENGINE    8192
#define STACK_FLIGHT_RECORDER   8192
#define STACK_MQTT_TELEMATICS   8192
#define STACK_TINYML_ANOMALY    8192
#define STACK_WEB_DASHBOARD     6144

// Task Priorities
#define PRIO_SPI_DMA_LISTENER   9
#define PRIO_CAN_TP             8
#define PRIO_UDS_SERVER         7
#define PRIO_AES_GCM_ENGINE     6
#define PRIO_FLIGHT_RECORDER    5
#define PRIO_WEB_DASHBOARD      5
#define PRIO_MQTT_TELEMATICS    4
#define PRIO_TINYML_ANOMALY     3

#endif // APP_CONFIG_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
extern QueueHandle_t violation_queue;
extern QueueHandle_t violation_queue_ws;
extern QueueHandle_t encrypted_queue;
extern QueueHandle_t telemetry_queue;
extern QueueHandle_t can_rx_queue;
