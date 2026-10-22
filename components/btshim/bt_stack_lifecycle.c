#include "bt_stack_lifecycle.h"
#include <esp_bt.h>
#include <esp_bt_main.h>

esp_err_t bt_stack_deinit(void) {
    esp_err_t result;
    if(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
        result = esp_bluedroid_disable();
        if(result != ESP_OK) return result;
    }
    if(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
        result = esp_bluedroid_deinit();
        if(result != ESP_OK) return result;
    }
    if(esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_UNINITIALIZED)
        return ESP_ERR_INVALID_STATE;

    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        result = esp_bt_controller_disable();
        if(result != ESP_OK) return result;
    }
    /* disable() leaves INITED, not IDLE. deinit() is what returns the allocated
     * controller memory; checking IDLE here used to skip it on every shutdown. */
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
        result = esp_bt_controller_deinit();
        if(result != ESP_OK) return result;
    }
    return esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE ?
               ESP_OK : ESP_ERR_INVALID_STATE;
}
