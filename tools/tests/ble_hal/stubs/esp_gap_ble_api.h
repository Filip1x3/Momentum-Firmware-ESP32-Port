#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef uint8_t esp_bd_addr_t[6];
typedef struct {
    int adv_int_min, adv_int_max, adv_type, own_addr_type, channel_map, adv_filter_policy;
} esp_ble_adv_params_t;
enum {ADV_TYPE_IND, BLE_ADDR_TYPE_RANDOM, ADV_CHNL_ALL, ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY};
typedef enum {
    ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT,
    ESP_GAP_BLE_ADV_START_COMPLETE_EVT,
    ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT,
    ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT,
} esp_gap_ble_cb_event_t;
enum {ESP_BT_STATUS_SUCCESS};
typedef struct {
    struct {int status;} adv_data_raw_cmpl;
    struct {int status;} adv_start_cmpl;
} esp_ble_gap_cb_param_t;
typedef void (*esp_gap_ble_cb_t)(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t*);
esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t callback);
esp_err_t esp_ble_gap_start_advertising(esp_ble_adv_params_t* params);
esp_err_t esp_ble_gap_stop_advertising(void);
esp_err_t esp_ble_gap_config_adv_data_raw(uint8_t* data, uint8_t size);
esp_err_t esp_ble_gap_set_rand_addr(esp_bd_addr_t address);
