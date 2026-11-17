// No radio, payload, advertising or frame generation is exercised by this test.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_log.h>
#include <btshim.h>
#include <bt_stack_lifecycle.h>
#include "bt_policy_under_test.h"
#ifdef BLE_TEST_WALK
#include <esp_gattc_api.h>
#define TAG "BleWalkHal"
#define WALK_GATTC_APP_ID 0
static void walk_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* parameter) {
    (void)event; (void)parameter;
    assert(!"Scanning callbacks are outside the lifecycle test");
}
static void walk_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t interface,
                                   esp_ble_gattc_cb_param_t* parameter) {
    (void)event; (void)interface; (void)parameter;
    assert(!"GATT traffic is outside the lifecycle test");
}
bool ble_walk_hal_start(void);
void ble_walk_hal_stop(void);
#include "walk_lifecycle_under_test.h"
#define ble_spam_hal_start ble_walk_hal_start
#define ble_spam_hal_stop ble_walk_hal_stop
#else
#include "../../../applications/main/ble_spam/ble_spam_hal.c"
#endif

static esp_bt_controller_status_t controller;
static esp_bluedroid_status_t host;
static esp_gap_ble_cb_t gap_callback;
static bool settings_enabled;
static int init_step, fail_init_at, profile_restores, profile_stops, service_stops;
static int service_restores, record_count, delays, reset_count, reinit_count;
static char fail_teardown_at;
static unsigned teardown_calls;
static TestApiLock message_lock;
static Bt service;
const void* ble_profile_serial = (void*)1;

const char* esp_err_to_name(esp_err_t error) {return error == ESP_OK ? "OK" : "test failure";}
esp_bt_controller_status_t esp_bt_controller_get_status(void) {return controller;}
esp_bluedroid_status_t esp_bluedroid_get_status(void) {return host;}
static esp_err_t init_result(void) {return ++init_step == fail_init_at ? ESP_FAIL : ESP_OK;}
static esp_err_t teardown_result(char action) {
    teardown_calls++;
    return action == fail_teardown_at ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_bt_controller_init(const esp_bt_controller_config_t* config) {
    assert(config && controller == ESP_BT_CONTROLLER_STATUS_IDLE);
    esp_err_t result = init_result();
    if(result == ESP_OK) controller = ESP_BT_CONTROLLER_STATUS_INITED;
    return result;
}
esp_err_t esp_bt_controller_enable(int mode) {
    assert(mode == ESP_BT_MODE_BLE && controller == ESP_BT_CONTROLLER_STATUS_INITED);
    esp_err_t result = init_result();
    if(result == ESP_OK) controller = ESP_BT_CONTROLLER_STATUS_ENABLED;
    return result;
}
esp_err_t esp_bluedroid_init_with_cfg(const esp_bluedroid_config_t* config) {
    assert(config && controller == ESP_BT_CONTROLLER_STATUS_ENABLED);
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED);
    esp_err_t result = init_result();
    if(result == ESP_OK) host = ESP_BLUEDROID_STATUS_INITIALIZED;
    return result;
}
esp_err_t esp_bluedroid_enable(void) {
    assert(host == ESP_BLUEDROID_STATUS_INITIALIZED);
    esp_err_t result = init_result();
    if(result == ESP_OK) host = ESP_BLUEDROID_STATUS_ENABLED;
    return result;
}
esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t callback) {
    assert(host == ESP_BLUEDROID_STATUS_ENABLED && callback);
    esp_err_t result = init_result();
    if(result == ESP_OK) gap_callback = callback;
    return result;
}
esp_err_t esp_bluedroid_disable(void) {
    assert(host == ESP_BLUEDROID_STATUS_ENABLED);
    esp_err_t result = teardown_result('H');
    if(result == ESP_OK) host = ESP_BLUEDROID_STATUS_INITIALIZED;
    return result;
}
esp_err_t esp_bluedroid_deinit(void) {
    assert(host == ESP_BLUEDROID_STATUS_INITIALIZED);
    esp_err_t result = teardown_result('h');
    if(result == ESP_OK) {host = ESP_BLUEDROID_STATUS_UNINITIALIZED; gap_callback = NULL;}
    return result;
}
esp_err_t esp_bt_controller_disable(void) {
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED);
    assert(controller == ESP_BT_CONTROLLER_STATUS_ENABLED);
    esp_err_t result = teardown_result('C');
    if(result == ESP_OK) controller = ESP_BT_CONTROLLER_STATUS_INITED;
    return result;
}
esp_err_t esp_bt_controller_deinit(void) {
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED);
    assert(controller == ESP_BT_CONTROLLER_STATUS_INITED);
    esp_err_t result = teardown_result('c');
    if(result == ESP_OK) controller = ESP_BT_CONTROLLER_STATUS_IDLE;
    return result;
}
esp_err_t esp_ble_tx_power_set(int type, int power) {
    (void)type; (void)power; return ESP_OK; // Configuration stub, never a radio.
}
esp_err_t esp_ble_gap_set_rand_addr(esp_bd_addr_t address) {
    assert(address && gap_callback && host == ESP_BLUEDROID_STATUS_ENABLED);
    esp_ble_gap_cb_param_t parameter = {0};
    gap_callback(ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT, &parameter);
    return ESP_OK;
}
esp_err_t esp_ble_gap_start_advertising(esp_ble_adv_params_t* params) {
    (void)params; assert(!"Advertising must never be started in this lifecycle test"); return ESP_FAIL;
}
esp_err_t esp_ble_gap_stop_advertising(void) {
    assert(!"No advertising exists in this lifecycle test"); return ESP_FAIL;
}
esp_err_t esp_ble_gap_config_adv_data_raw(uint8_t* data, uint8_t size) {
    (void)data; (void)size; assert(!"Payload construction/configuration is outside this test"); return ESP_FAIL;
}
void furi_hal_random_fill_buf(void* buffer, size_t size) {memset(buffer, 0, size);}
void furi_delay_ms(uint32_t delay) {assert(delay <= 100); delays++;}
void* furi_record_open(const char* name) {assert(strcmp(name, RECORD_BT) == 0); record_count++; return &service;}
void furi_record_close(const char* name) {assert(strcmp(name, RECORD_BT) == 0 && record_count > 0); record_count--;}
FuriApiLock api_lock_alloc_locked(void) {assert(!message_lock.unlocked); return &message_lock;}
void api_lock_wait_unlock_and_free(FuriApiLock lock) {assert(lock == &message_lock && lock->unlocked); lock->unlocked = false;}
int furi_message_queue_put(void* queue, const void* data, uint32_t timeout) {
    assert(queue == service.message_queue && timeout == FuriWaitForever);
    BtMessage* message = (BtMessage*)data;
    if(message->type == BtMessageTypeStopStack) {service_stops++; bt_handle_stop_stack(&service);}
    else {assert(message->type == BtMessageTypeRestoreDefault); service_restores++; bt_handle_restore_default(&service, message);}
    message->lock->unlocked = true;
    return FuriStatusOk;
}
void bt_settings_load(BtSettings* settings) {settings->enabled = settings_enabled;}
void bt_close_rpc_connection(Bt* bt) {assert(bt == &service);}
void furi_hal_bt_stop_advertising(void) {} // Service cleanup leaf; no RF implementation.
void ble_serial_reset_initialized(void) {reset_count++;}
void furi_hal_bt_reinit(void) {reinit_count++;}
static void profile_stop(TestProfile* profile) {assert(profile == service.current_profile); profile_stops++;}
static const TestProfileConfig default_config = {.stop = profile_stop};
static TestProfile default_profile = {.config = &default_config};
static void bt_change_profile(Bt* bt, BtMessage* message) {
    // Only this hardware leaf is mocked; the real restore policy must gate it.
    assert(bt == &service && settings_enabled && bt->bt_settings.enabled);
    assert(message->data.profile.template == ble_profile_serial);
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED && controller == ESP_BT_CONTROLLER_STATUS_IDLE);
    profile_restores++;
    host = ESP_BLUEDROID_STATUS_ENABLED;
    controller = ESP_BT_CONTROLLER_STATUS_ENABLED;
    bt->current_profile = &default_profile;
    if(message->result) *message->result = true;
}

#ifdef BLE_TEST_WALK
esp_err_t esp_ble_gattc_register_callback(esp_gattc_cb_t callback) {
    assert(callback && host == ESP_BLUEDROID_STATUS_ENABLED);
    return init_result();
}
esp_err_t esp_ble_gattc_app_register(unsigned app_id) {
    assert(app_id == WALK_GATTC_APP_ID && host == ESP_BLUEDROID_STATUS_ENABLED);
    esp_err_t result = init_result();
    // Simulate successful asynchronous REG_EVT, or its failure/absence at point 8.
    if(result == ESP_OK && fail_init_at != 8) s_gattc_if = 1;
    return result;
}
esp_err_t esp_ble_gattc_app_unregister(esp_gatt_if_t interface) {
    assert(interface != ESP_GATT_IF_NONE && host == ESP_BLUEDROID_STATUS_ENABLED);
    return ESP_OK;
}
esp_err_t esp_ble_gattc_close(esp_gatt_if_t interface, uint16_t connection) {
    (void)connection;
    assert(interface != ESP_GATT_IF_NONE && host == ESP_BLUEDROID_STATUS_ENABLED);
    s_connected = false; return ESP_OK;
}
esp_err_t esp_ble_gap_stop_scanning(void) {
    assert(host == ESP_BLUEDROID_STATUS_ENABLED); s_scanning = false; return ESP_OK;
}
esp_err_t esp_ble_gatt_set_local_mtu(unsigned mtu) {
    assert(mtu == 200 && s_gattc_if != ESP_GATT_IF_NONE);
    return ESP_OK;
}
#endif

static void assert_idle(void) {
#ifdef BLE_TEST_WALK
    assert(!s_stack_owned && !s_hal_started && !s_scanning && !s_connected);
    assert(s_gattc_if == ESP_GATT_IF_NONE);
#else
    assert(!s_stack_owned && !s_advertising && !s_adv_configured && !s_rand_addr_pending);
#endif
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED && controller == ESP_BT_CONTROLLER_STATUS_IDLE);
    assert(record_count == 0 && service.current_profile == NULL);
}
static void reset_case(bool enabled) {
    assert_idle(); settings_enabled = enabled;
    service = (Bt){.message_queue = (void*)1};
    init_step = fail_init_at = 0; fail_teardown_at = 0;
    profile_restores = profile_stops = service_stops = service_restores = 0;
    delays = reset_count = reinit_count = 0; teardown_calls = 0;
}
static void successful_stop(bool enabled) {
    int restores = profile_restores;
    ble_spam_hal_stop();
    assert(!s_stack_owned && record_count == 0);
#ifndef BLE_TEST_WALK
    assert(!s_rand_addr_pending);
#endif
    if(enabled) {
        assert(profile_restores == restores + 1 && service.current_profile == &default_profile);
        assert(host == ESP_BLUEDROID_STATUS_ENABLED && controller == ESP_BT_CONTROLLER_STATUS_ENABLED);
        // Avoid leaving the mocked default profile alive across test cases.
        settings_enabled = false;
        assert(bt_profile_restore_default(&service));
    } else assert(profile_restores == restores);
    assert_idle();
}

#ifndef BLE_TEST_WALK
int main(void) {
    service.message_queue = (void*)1;
    for(int failure = 1; failure <= 5; failure++) {
        reset_case(false); fail_init_at = failure;
        assert(!ble_spam_hal_start());
        assert(init_step == failure && service_restores == 1);
        assert_idle();
        unsigned released = teardown_calls;
        ble_spam_hal_stop(); assert(teardown_calls == released); // Idempotent after failure.
        fail_init_at = init_step = 0;
        assert(ble_spam_hal_start()); successful_stop(false);
    }
    for(int cycle = 0; cycle < 1000; cycle++) {
        bool enabled = (cycle % 2) != 0;
        reset_case(enabled);
        assert(ble_spam_hal_start());
        assert(s_stack_owned && init_step == 5 && !s_rand_addr_pending && !s_advertising);
        assert(host == ESP_BLUEDROID_STATUS_ENABLED && controller == ESP_BT_CONTROLLER_STATUS_ENABLED);
        successful_stop(enabled);
    }
    const char stages[] = {'H', 'h', 'C', 'c'};
    for(unsigned i = 0; i < sizeof(stages); i++) {
        reset_case(false);
        assert(ble_spam_hal_start()); fail_teardown_at = stages[i];
        ble_spam_hal_stop();
        assert(s_stack_owned && service_restores == 0);
        int old_init = init_step;
        assert(!ble_spam_hal_start()); // Incomplete teardown must block another host init.
        assert(s_stack_owned && init_step == old_init && service_restores == 0);
        fail_teardown_at = 0; init_step = 0;
        assert(ble_spam_hal_start()); // Resume teardown, then reopen safely.
        successful_stop(false);
    }
    reset_case(false);
    fail_init_at = 2; fail_teardown_at = 'c';
    assert(!ble_spam_hal_start()); // Controller enable failed; its allocation still needs deinit.
    assert(s_stack_owned && controller == ESP_BT_CONTROLLER_STATUS_INITED);
    assert(host == ESP_BLUEDROID_STATUS_UNINITIALIZED && service_restores == 0);
    assert(!ble_spam_hal_start());
    assert(s_stack_owned && init_step == 2 && record_count == 0);
    fail_teardown_at = 0;
    ble_spam_hal_stop(); assert_idle();
    fail_init_at = init_step = 0;
    assert(ble_spam_hal_start()); successful_stop(false);
    reset_case(false);
    host = ESP_BLUEDROID_STATUS_ENABLED; controller = ESP_BT_CONTROLLER_STATUS_ENABLED;
    fail_teardown_at = 'H';
    assert(!ble_spam_hal_start()); // Another service still owns a non-released stack.
    assert(!s_stack_owned && init_step == 0 && service_restores == 0);
    fail_teardown_at = 0;
    assert(ble_spam_hal_start()); successful_stop(false);
    reset_case(false);
    assert(ble_spam_hal_start()); init_step = 0;
    assert(ble_spam_hal_start()); // Repeated start first cleans the previously owned stack.
    assert(s_stack_owned && init_step == 5);
    successful_stop(false);
    puts("PASS: real BLE HAL + teardown + restore policy; 5 init failures, 1000 cycles, 4 partial teardown/retries, BT-disabled preserved; no RF/payloads");
    return 0;
}
#else
int main(void) {
    service.message_queue = (void*)1;
    for(int failure = 1; failure <= 8; failure++) {
        reset_case(false); fail_init_at = failure;
        assert(!ble_walk_hal_start());
        assert(init_step == (failure == 8 ? 7 : failure) && service_restores == 1);
        assert_idle();
        if(failure == 8) assert(delays == 41); // 100ms handoff + 40 bounded wait steps.
        unsigned released = teardown_calls;
        ble_walk_hal_stop(); assert(teardown_calls == released);
        fail_init_at = init_step = 0;
        assert(ble_walk_hal_start()); successful_stop(false);
    }
    for(int cycle = 0; cycle < 1000; cycle++) {
        bool enabled = (cycle % 2) != 0;
        reset_case(enabled);
        assert(ble_walk_hal_start());
        assert(s_stack_owned && s_hal_started && init_step == 7);
        int initialized = init_step, stopped = service_stops;
        assert(ble_walk_hal_start()); // Already-started path remains idempotent.
        assert(init_step == initialized && service_stops == stopped);
        // Only flags are simulated: no scanning or connections are initiated.
        s_connected = true; s_scanning = true;
        successful_stop(enabled);
    }
    const char stages[] = {'H', 'h', 'C', 'c'};
    for(unsigned i = 0; i < sizeof(stages); i++) {
        reset_case(false); assert(ble_walk_hal_start());
        fail_teardown_at = stages[i];
        ble_walk_hal_stop();
        assert(s_stack_owned && !s_hal_started && service_restores == 0);
        int old_init = init_step;
        assert(!ble_walk_hal_start());
        assert(s_stack_owned && init_step == old_init && service_restores == 0);
        fail_teardown_at = 0; init_step = 0;
        assert(ble_walk_hal_start()); successful_stop(false);
    }
    reset_case(false); fail_init_at = 2; fail_teardown_at = 'c';
    assert(!ble_walk_hal_start());
    assert(s_stack_owned && !s_hal_started && init_step == 2);
    assert(!ble_walk_hal_start());
    fail_teardown_at = 0; ble_walk_hal_stop(); assert_idle();
    reset_case(false);
    host = ESP_BLUEDROID_STATUS_ENABLED; controller = ESP_BT_CONTROLLER_STATUS_ENABLED;
    fail_teardown_at = 'H';
    assert(!ble_walk_hal_start());
    assert(!s_stack_owned && init_step == 0);
    fail_teardown_at = 0;
    assert(ble_walk_hal_start()); successful_stop(false);
    puts("PASS: real BLE Walk lifecycle; 7 init failures + REG timeout, 1000 cycles/idempotent starts, 4 partial teardowns/retries, BT OFF preserved; no RF/GATT traffic");
    return 0;
}
#endif
