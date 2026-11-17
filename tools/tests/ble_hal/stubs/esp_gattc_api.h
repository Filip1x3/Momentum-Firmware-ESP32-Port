#pragma once
#include "esp_gap_ble_api.h"
typedef int esp_gatt_if_t;
#define ESP_GATT_IF_NONE 255
typedef int esp_gattc_cb_event_t;
typedef struct {int unused;} esp_ble_gattc_cb_param_t;
typedef void (*esp_gattc_cb_t)(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t*);
esp_err_t esp_ble_gattc_register_callback(esp_gattc_cb_t callback);
esp_err_t esp_ble_gattc_app_register(unsigned app_id);
esp_err_t esp_ble_gattc_app_unregister(esp_gatt_if_t interface);
esp_err_t esp_ble_gattc_close(esp_gatt_if_t interface, uint16_t connection);
esp_err_t esp_ble_gatt_set_local_mtu(unsigned mtu);
esp_err_t esp_ble_gap_stop_scanning(void);
