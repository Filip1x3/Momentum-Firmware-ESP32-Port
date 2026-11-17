#pragma once
#include "esp_err.h"
typedef enum {
    ESP_BLUEDROID_STATUS_UNINITIALIZED,
    ESP_BLUEDROID_STATUS_INITIALIZED,
    ESP_BLUEDROID_STATUS_ENABLED,
} esp_bluedroid_status_t;
typedef struct {int unused;} esp_bluedroid_config_t;
#define BT_BLUEDROID_INIT_CONFIG_DEFAULT() {0}
esp_bluedroid_status_t esp_bluedroid_get_status(void);
esp_err_t esp_bluedroid_init_with_cfg(const esp_bluedroid_config_t* config);
esp_err_t esp_bluedroid_enable(void);
esp_err_t esp_bluedroid_disable(void);
esp_err_t esp_bluedroid_deinit(void);
