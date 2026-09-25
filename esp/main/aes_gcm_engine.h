#ifndef AES_GCM_ENGINE_H
#define AES_GCM_ENGINE_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct __attribute__((packed)) {
    uint8_t  iv[12];          // AES-GCM IV (nonce)
    uint8_t  ciphertext[64];  // Encrypted violation data
    uint8_t  tag[16];         // GCM authentication tag
    uint32_t sequence_num;    // Monotonic counter (anti-replay)
    int64_t  esp_timestamp;   // ESP32 local timestamp
} encrypted_record_t;

void task_aes_gcm_engine(void *pvParameters);

#endif // AES_GCM_ENGINE_H
