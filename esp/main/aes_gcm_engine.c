#include "aes_gcm_engine.h"
#include "app_config.h"
#include "esp_random.h"
#include "spi_dma_listener.h"
#include "event_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mbedtls/gcm.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "AES_GCM";

// Hardcoded 256-bit key for prototype
static const uint8_t aes_key[32] = {
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef
};

void task_aes_gcm_engine(void *pvParameters) {
    ESP_LOGI(TAG, "Starting AES-GCM engine task on core %d", xPortGetCoreID());
    
    uint32_t sequence_num = 0;
    
    // Attempt to load sequence_num from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        esp_err_t load = nvs_get_u32(nvs_handle, "seq_num", &sequence_num);
        if (load != ESP_OK && load != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGW(TAG, "Sequence load failed: %s", esp_err_to_name(load));
    } else {
        ESP_LOGW(TAG, "NVS unavailable; sequence will not persist");
    }
    
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, aes_key, 256) != 0) {
        ESP_LOGE(TAG, "AES key setup failed; encryption task stopped");
        mbedtls_gcm_free(&gcm);
        if (err == ESP_OK) nvs_close(nvs_handle);
        vTaskDelete(NULL);
        return;
    }
    
    violation_descriptor_t raw_desc;
    
    while (1) {
        if (xQueueReceive(violation_queue, &raw_desc, portMAX_DELAY) == pdTRUE) {
            encrypted_record_t record = {0};
            record.sequence_num = sequence_num++;
            record.esp_timestamp = raw_desc.esp_timestamp_us;
            
            // Save sequence_num to NVS periodically or on every message
            if (err == ESP_OK) {
                esp_err_t saved = nvs_set_u32(nvs_handle, "seq_num", sequence_num);
                if (saved == ESP_OK) saved = nvs_commit(nvs_handle);
                if (saved != ESP_OK)
                    ESP_LOGW(TAG, "Sequence persistence failed: %s", esp_err_to_name(saved));
            }
            
            // Generate 12-byte IV: 8 bytes random + 4 bytes sequence counter
            esp_fill_random(record.iv, 8);
            memcpy(&record.iv[8], &record.sequence_num, 4);
            
            uint8_t plaintext[64];
            encode_event_plaintext(&raw_desc, plaintext);
            for (unsigned i = 0; i < 4; ++i)
                plaintext[20+i] = (uint8_t)(record.sequence_num >> (24-8*i));
            memset(record.ciphertext, 0, sizeof(record.ciphertext));
            
            // mbedtls hardware accelerated on ESP32-S3
            int crypt_result = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, sizeof(plaintext),
                                      record.iv, sizeof(record.iv),
                                      NULL, 0,
                                      plaintext, record.ciphertext,
                                      sizeof(record.tag), record.tag);
                                      
            if (crypt_result != 0) {
                ESP_LOGE(TAG, "AES-GCM encryption failed: %d", crypt_result);
                continue;
            }
            if (xQueueSend(encrypted_queue, &record, pdMS_TO_TICKS(10)) != pdTRUE)
                ESP_LOGW(TAG, "Encrypted record queue full; record dropped");
            
            // Format JSON telemetry string
            
            char json_buf[128];
            snprintf(json_buf, sizeof(json_buf), 
                     "{\"ts\":%lu,\"id\":\"0x%lx\",\"ext\":%s,\"err\":%u,\"seq\":%lu,\"source\":%u}",
                     (unsigned long)raw_desc.timestamp_24, (unsigned long)raw_desc.arb_id,
                     raw_desc.is_extended ? "true" : "false", raw_desc.error_code,
                     (unsigned long)record.sequence_num, raw_desc.source);
            
            if (xQueueSend(telemetry_queue, json_buf, pdMS_TO_TICKS(10)) != pdTRUE)
                ESP_LOGW(TAG, "MQTT queue full; telemetry dropped");
        }
    }
    
    mbedtls_gcm_free(&gcm);
    if (err == ESP_OK) {
        nvs_close(nvs_handle);
    }
    vTaskDelete(NULL);
}
