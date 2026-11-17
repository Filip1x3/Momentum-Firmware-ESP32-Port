/* Run extracted production RX callback/teardown only; no RF hardware or TX. */
#include <windows.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define UNUSED(x) (void)(x)
#define furi_check(x) assert(x)
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_D(...) ((void)0)
#define FuriWaitForever UINT32_MAX
#define FuriStatusOk 0
#define ProtoPirateTxRxStateIDLE 0
#define ProtoPirateTxRxStateRx 1
#define ProtoPirateTxRxStateTx 2
typedef struct { CRITICAL_SECTION section; unsigned depth; } FuriMutex;
typedef struct { unsigned count; } ItemArray;
typedef struct { ItemArray data; } ProtoPirateHistory;
typedef struct { FuriMutex* mutex; } View;
typedef struct { View* view; } ProtoPirateReceiver;
typedef struct { bool running, freed; } SubGhzWorker;
typedef struct { bool freed; } SubGhzReceiver;
typedef struct { const char* name; } Protocol;
typedef struct { Protocol* protocol; } SubGhzProtocolDecoderBase;
typedef struct { bool async_running; } Radio;
typedef struct { int unused; } Preset;
typedef struct {
    SubGhzWorker* worker;
    SubGhzReceiver* receiver;
    Radio* radio_device;
    ProtoPirateHistory* history;
    Preset* preset;
    int txrx_state;
} ProtoPirateTxRx;
typedef struct {
    ProtoPirateTxRx* txrx;
    ProtoPirateReceiver* protopirate_receiver;
    bool auto_save, check_saved, receiver_history_dirty;
} ProtoPirateApp;
static unsigned joined, receiver_freed, saved_pending, match_pending;
static FuriMutex* model_mutex;
static void* view_get_model(View* view) {
    FuriMutex* mutex = view->mutex;
    EnterCriticalSection(&mutex->section);
    assert(!mutex->depth); /* Real view mutex is not recursive. */
    mutex->depth++; return view;
}
static void view_commit_model(View* view, bool update) {
    FuriMutex* mutex = view->mutex;
    assert(!update);
    assert(mutex->depth); mutex->depth--; LeaveCriticalSection(&mutex->section);
}
static unsigned ProtoPirateHistoryItemArray_size(ItemArray data) { return data.count; }
static bool protopirate_history_add_to_history(
    ProtoPirateHistory* history, SubGhzProtocolDecoderBase* decoder, Preset* preset) {
    UNUSED(decoder); UNUSED(preset); assert(model_mutex->depth);
    if(history->data.count == 20) return false;
    history->data.count++; return true;
}
static void protopirate_history_mark_auto_save_pending(ProtoPirateHistory* history, uint16_t idx) {
    assert(model_mutex->depth && idx < history->data.count); saved_pending++;
}
static void protopirate_history_mark_saved_match_pending(ProtoPirateHistory* history, uint16_t idx) {
    assert(model_mutex->depth && idx < history->data.count); match_pending++;
}
static bool subghz_worker_is_running(SubGhzWorker* worker) { return worker->running; }
static void subghz_worker_stop(SubGhzWorker* worker) {
    assert(worker->running && !worker->freed); worker->running = false; joined++;
}
static void subghz_worker_free(SubGhzWorker* worker) {
    assert(!worker->running && !worker->freed); worker->freed = true;
}
static void subghz_receiver_set_rx_callback(SubGhzReceiver* receiver, void* callback, void* context) {
    assert(!receiver->freed && callback == NULL && context == NULL);
}
static void subghz_receiver_free(SubGhzReceiver* receiver) {
    assert(!receiver->freed && joined); receiver->freed = true; receiver_freed++;
}
static void subghz_devices_stop_async_rx(Radio* radio) {
    assert(radio->async_running); radio->async_running = false;
}
static void subghz_devices_idle(Radio* radio) { assert(!radio->async_running); }
#include "production_receive.h"

static DWORD WINAPI capture_thread(void* context) {
    ProtoPirateApp* app = context;
    Protocol protocol = {"test"}; SubGhzProtocolDecoderBase decoder = {&protocol};
    for(unsigned i = 0; i < 100000; i++)
        protopirate_scene_receiver_callback(NULL, &decoder, app);
    return 0;
}
int main(void) {
    FuriMutex mutex = {0}; InitializeCriticalSection(&mutex.section);
    model_mutex = &mutex;
    ProtoPirateHistory history = {{0}};
    View view = {&mutex}; ProtoPirateReceiver receiver_view = {&view};
    Preset preset = {0}; ProtoPirateTxRx txrx = {0};
    txrx.history = &history; txrx.preset = &preset;
    ProtoPirateApp app = {&txrx, &receiver_view, true, true, false};
    HANDLE thread = CreateThread(NULL, 0, capture_thread, &app, 0, NULL);
    assert(thread);
    for(unsigned i = 0; i < 100000; i++) {
        protopirate_view_receiver_lock_history(&receiver_view);
        assert(protopirate_history_get_item(&history) <= 20);
        if(history.data.count) history.data.count--; /* GUI delete under same lock */
        app.receiver_history_dirty = false;
        protopirate_view_receiver_unlock_history(&receiver_view);
    }
    assert(WaitForSingleObject(thread, 30000) == WAIT_OBJECT_0); CloseHandle(thread);
    assert(!mutex.depth && saved_pending == match_pending && saved_pending);
    DeleteCriticalSection(&mutex.section);
    for(unsigned i = 0; i < 10000; i++) {
        for(unsigned started = 0; started < 2; started++) {
            SubGhzWorker worker = {true, false}; SubGhzReceiver receiver = {false};
            Radio radio = {started != 0};
            txrx.worker = &worker; txrx.receiver = &receiver; txrx.radio_device = &radio;
            txrx.txrx_state = started ? ProtoPirateTxRxStateRx : ProtoPirateTxRxStateIDLE;
            joined = 0;
            protopirate_rx_stack_teardown_for_registry_switch(&app);
            assert(joined == 1 && receiver.freed && worker.freed && !radio.async_running);
            assert(txrx.worker == NULL && txrx.receiver == NULL);
        }
    }
    assert(receiver_freed == 20000);
    puts("RX: 100000 concurrent capture/delete/read cycles; 20000 active/partial teardowns PASS");
    return 0;
}
