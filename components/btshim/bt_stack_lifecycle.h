#pragma once

#include <esp_err.h>

/* Caller must first close RPC and stop/free the active BLE profile. Stops the
 * host before its controller and releases their allocations, but never calls
 * irreversible esp_bt_mem_release(ESP_BT_MODE_BLE). Bluetooth can be restarted. */
esp_err_t bt_stack_deinit(void);
