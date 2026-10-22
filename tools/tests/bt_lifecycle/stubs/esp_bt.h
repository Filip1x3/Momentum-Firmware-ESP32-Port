#pragma once
#include "esp_err.h"
typedef enum {
    ESP_BT_CONTROLLER_STATUS_IDLE,
    ESP_BT_CONTROLLER_STATUS_INITED,
    ESP_BT_CONTROLLER_STATUS_ENABLED,
} esp_bt_controller_status_t;
esp_bt_controller_status_t esp_bt_controller_get_status(void);
esp_err_t esp_bt_controller_disable(void);
esp_err_t esp_bt_controller_deinit(void);
