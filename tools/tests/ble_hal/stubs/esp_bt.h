#pragma once
#include "esp_err.h"
typedef enum {
    ESP_BT_CONTROLLER_STATUS_IDLE,
    ESP_BT_CONTROLLER_STATUS_INITED,
    ESP_BT_CONTROLLER_STATUS_ENABLED,
} esp_bt_controller_status_t;
typedef struct {int unused;} esp_bt_controller_config_t;
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() {0}
#define ESP_BT_MODE_BLE 1
#define ESP_PWR_LVL_P20 20
enum {ESP_BLE_PWR_TYPE_ADV, ESP_BLE_PWR_TYPE_DEFAULT};
esp_bt_controller_status_t esp_bt_controller_get_status(void);
esp_err_t esp_bt_controller_init(const esp_bt_controller_config_t* config);
esp_err_t esp_bt_controller_enable(int mode);
esp_err_t esp_bt_controller_disable(void);
esp_err_t esp_bt_controller_deinit(void);
esp_err_t esp_ble_tx_power_set(int type, int power);
