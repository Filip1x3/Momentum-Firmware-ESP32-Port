#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include "../../../components/btshim/bt_stack_lifecycle.h"

static esp_bt_controller_status_t controller;
static esp_bluedroid_status_t host;
static char trace[16];
static int count, fail_at;

static esp_err_t step(char action) {
    trace[count++] = action;
    return count == fail_at ? ESP_FAIL : ESP_OK;
}
esp_bt_controller_status_t esp_bt_controller_get_status(void) { return controller; }
esp_bluedroid_status_t esp_bluedroid_get_status(void) { return host; }
esp_err_t esp_bluedroid_disable(void) {
    assert(host == ESP_BLUEDROID_STATUS_ENABLED);
    esp_err_t result = step('H');
    if(result == ESP_OK) host = ESP_BLUEDROID_STATUS_INITIALIZED;
    return result;
}
esp_err_t esp_bluedroid_deinit(void) {
    assert(host == ESP_BLUEDROID_STATUS_INITIALIZED);
    esp_err_t result = step('h');
    if(result == ESP_OK) host = ESP_BLUEDROID_STATUS_UNINITIALIZED;
    return result;
}
esp_err_t esp_bt_controller_disable(void) {
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED);
    assert(controller == ESP_BT_CONTROLLER_STATUS_ENABLED);
    esp_err_t result = step('C');
    if(result == ESP_OK) controller = ESP_BT_CONTROLLER_STATUS_INITED;
    return result;
}
esp_err_t esp_bt_controller_deinit(void) {
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED);
    assert(controller == ESP_BT_CONTROLLER_STATUS_INITED);
    esp_err_t result = step('c');
    if(result == ESP_OK) controller = ESP_BT_CONTROLLER_STATUS_IDLE;
    return result;
}
static void reset(int h, int c, int fail) {
    host = (esp_bluedroid_status_t)h;
    controller = (esp_bt_controller_status_t)c;
    count = 0;
    fail_at = fail;
    memset(trace, 0, sizeof(trace));
}

int main(void) {
    const char* host_calls[] = {"", "h", "Hh"};
    const char* controller_calls[] = {"", "c", "Cc"};
    for(int h = 0; h < 3; h++) for(int c = 0; c < 3; c++) {
        reset(h, c, 0);
        char expected[16];
        snprintf(expected, sizeof(expected), "%s%s", host_calls[h], controller_calls[c]);
        assert(bt_stack_deinit() == ESP_OK);
        assert(strcmp(trace, expected) == 0);
        assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED && controller == ESP_BT_CONTROLLER_STATUS_IDLE);
        assert(bt_stack_deinit() == ESP_OK);
        assert(strcmp(trace, expected) == 0); /* idempotent */
    }
    for(int fail = 1; fail <= 4; fail++) {
        reset(2, 2, fail);
        assert(bt_stack_deinit() == ESP_FAIL);
        assert(count == fail); /* never tear down controller beneath live host */
        fail_at = 0;
        assert(bt_stack_deinit() == ESP_OK); /* resumable after a failed step */
    }
    for(int cycle = 0; cycle < 10000; cycle++) {
        reset(2, 2, 0);
        assert(bt_stack_deinit() == ESP_OK);
        assert(strcmp(trace, "HhCc") == 0);
    }
    reset(3, 2, 0);
    assert(bt_stack_deinit() == ESP_ERR_INVALID_STATE && count == 0);
    reset(0, 3, 0);
    assert(bt_stack_deinit() == ESP_ERR_INVALID_STATE && count == 0);
    puts("PASS: 9 initial states, shutdown order, idempotence, 4 failure/retry paths, 10000 restartable cycles");
    return 0;
}
