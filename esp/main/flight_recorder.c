#include "flight_recorder.h"
#include "aes_gcm_engine.h"
#include "app_config.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <stdio.h>
#include <dirent.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <unistd.h>

static atomic_uint total_records, errors;
static atomic_bool ready;
static unsigned current_file_idx;
uint32_t flight_recorder_get_count(void) { return atomic_load(&total_records); }
uint32_t flight_recorder_errors(void) { return atomic_load(&errors); }
bool flight_recorder_ready(void) { return atomic_load(&ready); }

esp_err_t flight_recorder_init(void) {
    esp_vfs_littlefs_conf_t cfg = {
        .base_path = FLIGHT_LOG_MOUNT, .partition_label = FLIGHT_LOG_PARTITION,
        .format_if_mount_failed = false, .dont_mount = false,
    };
    esp_err_t err = esp_vfs_littlefs_register(&cfg);
    if (err != ESP_OK) {
        atomic_fetch_add(&errors, 1);
        ESP_LOGE("FLIGHT", "Mount failed; existing data preserved (no automatic format)");
        return err;
    }
    // Recover count from complete records, not stale metadata.
    DIR *dir = opendir(FLIGHT_LOG_MOUNT);
    if (!dir) { atomic_fetch_add(&errors, 1); return ESP_FAIL; }
    struct dirent *entry;
    unsigned count = 0, next_file = 0;
    while ((entry = readdir(dir))) {
        unsigned idx; int consumed = 0;
        if (sscanf(entry->d_name, "log_%x.bin%n", &idx, &consumed) != 1 ||
            consumed == 0 || entry->d_name[consumed] != '\0') continue;
        char path[320];
        snprintf(path, sizeof(path), "%s/%s", FLIGHT_LOG_MOUNT, entry->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        count += st.st_size / sizeof(encrypted_record_t);
        if (st.st_size % sizeof(encrypted_record_t)) atomic_fetch_add(&errors, 1);
        if (idx >= next_file) next_file = idx + 1;
    }
    closedir(dir);
    current_file_idx = next_file; // New file each boot; never append after a partial record.
    atomic_store(&total_records, count);
    atomic_store(&ready, true);
    return ESP_OK;
}
void task_flight_recorder(void *unused) {
    encrypted_record_t record;
    FILE *file = NULL;
    size_t file_bytes = 0;
    while (1) {
        if (xQueueReceive(encrypted_queue, &record, portMAX_DELAY) != pdTRUE) continue;
        if (!flight_recorder_ready()) { atomic_fetch_add(&errors, 1); continue; }
        if (!file) {
            char path[64];
            snprintf(path, sizeof(path), "%s/log_%06X.bin", FLIGHT_LOG_MOUNT, current_file_idx);
            file = fopen(path, "ab");
        }
        bool ok = file && fwrite(&record, sizeof(record), 1, file) == 1 &&
                  fflush(file) == 0 && fsync(fileno(file)) == 0;
        if (!ok) {
            atomic_fetch_add(&errors, 1);
            atomic_store(&ready, false);
            if (file) fclose(file);
            file = NULL;
            ESP_LOGE("FLIGHT", "Write/sync failed; logging stopped to preserve files");
            continue;
        }
        atomic_fetch_add(&total_records, 1);
        file_bytes += sizeof(record);
        if (file_bytes >= 4096) {
            if (fclose(file) != 0) {
                atomic_fetch_add(&errors, 1); atomic_store(&ready, false);
            }
            file = NULL; file_bytes = 0; ++current_file_idx;
        }
    }
}
