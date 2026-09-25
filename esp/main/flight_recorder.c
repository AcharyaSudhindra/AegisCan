#include "flight_recorder.h"
#include "aes_gcm_engine.h"
#include "app_config.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "FLIGHT_RECORDER";
static uint32_t total_records = 0;
static uint32_t current_file_idx = 0;

esp_err_t flight_recorder_init(void) {
    ESP_LOGI(TAG, "Initializing LittleFS on flight_log partition");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/flight",
        .partition_label = "flight_log",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format LittleFS");
        return ret;
    }

    FILE *f = fopen("/flight/meta.bin", "rb");
    if (f) {
        fread(&current_file_idx, sizeof(current_file_idx), 1, f);
        fread(&total_records, sizeof(total_records), 1, f);
        fclose(f);
        ESP_LOGI(TAG, "Loaded metadata: file_idx=%lu, records=%lu", current_file_idx, total_records);
    }

    return ESP_OK;
}

uint32_t flight_recorder_get_count(void) {
    return total_records;
}

void task_flight_recorder(void *pvParameters) {
    ESP_LOGI(TAG, "Starting flight recorder task on core %d", xPortGetCoreID());
    encrypted_record_t record;
    
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/flight/log_%06lX.bin", (unsigned long)current_file_idx);
    FILE *f = fopen(filepath, "ab");
    
    while (1) {
        if (xQueueReceive(encrypted_queue, &record, portMAX_DELAY) == pdTRUE) {
            if (!f) {
                snprintf(filepath, sizeof(filepath), "/flight/log_%06lX.bin", (unsigned long)current_file_idx);
                f = fopen(filepath, "ab");
            }
            
            if (f) {
                fwrite(&record, sizeof(record), 1, f);
                fflush(f);
                fsync(fileno(f)); // Flush/sync after every write for crash safety
                
                total_records++;
                
                struct stat st;
                if (stat(filepath, &st) == 0 && st.st_size >= 4096) { // 4KB rollover
                    fclose(f);
                    f = NULL;
                    current_file_idx++;
                    
                    FILE *meta = fopen("/flight/meta.bin", "wb");
                    if (meta) {
                        fwrite(&current_file_idx, sizeof(current_file_idx), 1, meta);
                        fwrite(&total_records, sizeof(total_records), 1, meta);
                        fclose(meta);
                    }
                }
            }
        }
    }
}
