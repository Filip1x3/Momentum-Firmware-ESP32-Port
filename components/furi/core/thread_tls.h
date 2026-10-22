#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* ESP-IDF reserves FreeRTOS TLS slot 0 for pthread's values_list_t. Wi-Fi,
 * Bluetooth coexistence and lwIP use it even from non-pthread tasks. Putting a
 * FuriThread* there makes pthread_getspecific() walk a TCB as a linked list.
 * Count usable slots, NOT configNUM_THREAD_LOCAL_STORAGE_POINTERS: IDF doubles
 * that latter value to hold deletion callbacks, not extra user pointers. */
#define FURI_THREAD_TLS_INDEX 1
#if CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS <= FURI_THREAD_TLS_INDEX
#error "Furi requires CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS >= 2 (slot 0 belongs to ESP-IDF pthread)"
#endif

static inline void* furi_thread_tls_get(TaskHandle_t task) {
    return pvTaskGetThreadLocalStoragePointer(task, FURI_THREAD_TLS_INDEX);
}

static inline void furi_thread_tls_set(TaskHandle_t task, void* thread) {
    vTaskSetThreadLocalStoragePointer(task, FURI_THREAD_TLS_INDEX, thread);
}
