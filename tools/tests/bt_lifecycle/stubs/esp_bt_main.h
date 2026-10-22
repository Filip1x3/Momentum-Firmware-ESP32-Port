#pragma once
#include "esp_err.h"
typedef enum {
    ESP_BLUEDROID_STATUS_UNINITIALIZED,
    ESP_BLUEDROID_STATUS_INITIALIZED,
    ESP_BLUEDROID_STATUS_ENABLED,
} esp_bluedroid_status_t;
esp_bluedroid_status_t esp_bluedroid_get_status(void);
esp_err_t esp_bluedroid_disable(void);
esp_err_t esp_bluedroid_deinit(void);
