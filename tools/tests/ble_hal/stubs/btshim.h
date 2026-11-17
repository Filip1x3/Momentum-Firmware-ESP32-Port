#pragma once
#include "furi.h"
#define RECORD_BT "bt"
typedef struct {bool enabled;} BtSettings;
typedef struct TestProfile TestProfile;
typedef struct {void (*stop)(TestProfile* profile);} TestProfileConfig;
struct TestProfile {const TestProfileConfig* config;};
typedef struct {
    BtSettings bt_settings;
    TestProfile* current_profile;
    void* message_queue;
    int status;
} Bt;
typedef enum {BtMessageTypeRestoreDefault, BtMessageTypeStopStack} BtMessageType;
typedef struct {
    FuriApiLock lock;
    BtMessageType type;
    bool* result;
    struct {struct {const void* template; void* params;} profile;} data;
} BtMessage;
enum {BtStatusOff, BtStatusUnavailable};
extern const void* ble_profile_serial;
void bt_stop_stack(Bt* bt);
bool bt_profile_restore_default(Bt* bt);
void bt_settings_load(BtSettings* settings);
void bt_close_rpc_connection(Bt* bt);
void furi_hal_bt_stop_advertising(void);
void ble_serial_reset_initialized(void);
void furi_hal_bt_reinit(void);
static void bt_change_profile(Bt* bt, BtMessage* message);
