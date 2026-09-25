#include "aes_gcm_engine.h"
#include "app_config.h"
#include "esp_random.h"
#include "spi_dma_listener.h"
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
        nvs_get_u32(nvs_handle, "seq_num", &sequence_num);
    }
    
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, aes_key, 256);
    
    violation_descriptor_t raw_desc;
    
    while (1) {
        if (xQueueReceive(violation_queue, &raw_desc, portMAX_DELAY) == pdTRUE) {
            encrypted_record_t record;
            record.sequence_num = sequence_num++;
            record.esp_timestamp = esp_timer_get_time();
            
            // Save sequence_num to NVS periodically or on every message
            if (err == ESP_OK) {
                nvs_set_u32(nvs_handle, "seq_num", sequence_num);
                nvs_commit(nvs_handle);
            }
            
            // Generate 12-byte IV: 8 bytes random + 4 bytes sequence counter
            esp_fill_random(record.iv, 8);
            memcpy(&record.iv[8], &record.sequence_num, 4);
            
            uint8_t plaintext[sizeof(violation_descriptor_t)];
            memcpy(plaintext, &raw_desc, sizeof(violation_descriptor_t));
            memset(record.ciphertext, 0, sizeof(record.ciphertext));
            
            // mbedtls hardware accelerated on ESP32-S3
            mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, sizeof(plaintext),
                                      record.iv, sizeof(record.iv),
                                      NULL, 0,
                                      plaintext, record.ciphertext,
                                      sizeof(record.tag), record.tag);
                                      
            xQueueSend(encrypted_queue, &record, pdMS_TO_TICKS(10));
            
            // Format JSON telemetry string
            // Extract from 64-bit violation_descriptor_t (Assuming it's a uint64_t or union)
            uint64_t desc_val = *(uint64_t*)&raw_desc; 
            uint32_t ts = (desc_val >> 40) & 0xFFFFFF;
            bool ext = (desc_val >> 39) & 0x1;
            uint32_t arb_id = (desc_val >> 10) & 0x1FFFFFFF;
            uint8_t err_code = (desc_val >> 6) & 0xF;
            
            char json_buf[128];
            snprintf(json_buf, sizeof(json_buf), 
                     "{\"ts\":%lu,\"id\":\"0x%lx\",\"ext\":%s,\"err\":%u,\"seq\":%lu}",
                     (unsigned long)ts, (unsigned long)arb_id, ext ? "true" : "false", err_code, (unsigned long)record.sequence_num);
            
            xQueueSend(telemetry_queue, json_buf, pdMS_TO_TICKS(10));
        }
    }
    
    mbedtls_gcm_free(&gcm);
    if (err == ESP_OK) {
        nvs_close(nvs_handle);
    }
    vTaskDelete(NULL);
}

