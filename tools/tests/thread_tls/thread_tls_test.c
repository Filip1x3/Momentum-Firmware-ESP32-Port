#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "../../../components/furi/core/thread_tls.h"

struct MockTask {
    void* pointers[CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS];
    void* callbacks[CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS];
};
static struct MockTask* current;
void* pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, int slot) {
    assert(slot >= 0 && slot < CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS);
    return (task ? task : current)->pointers[slot];
}
void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, int slot, void* value) {
    assert(slot >= 0 && slot < CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS);
    (task ? task : current)->pointers[slot] = value;
}

int main(void) {
    int furi_identity, posix_values, deletion_callback;
    for(int iteration = 0; iteration < 10000; iteration++) {
        struct MockTask native = {0}, furi = {0};
        current = &native;
        native.pointers[0] = &posix_values;
        native.callbacks[0] = &deletion_callback;
        /* IDF-owned tasks are not mistaken for FuriThread objects. */
        assert(furi_thread_tls_get(NULL) == NULL);
        current = &furi;
        furi_thread_tls_set(NULL, &furi_identity);
        assert(furi_thread_tls_get(NULL) == &furi_identity);
        /* pthread_getspecific() sees no bogus Furi TCB at first use. */
        assert(pvTaskGetThreadLocalStoragePointer(NULL, 0) == NULL);
        /* ESP-IDF may register pthread data/destructors on this Furi task. */
        furi.pointers[0] = &posix_values;
        furi.callbacks[0] = &deletion_callback;
        assert(furi_thread_tls_get(NULL) == &furi_identity);
        current = &native;
        assert(furi_thread_tls_get(&furi) == &furi_identity);
        furi_thread_tls_set(&furi, NULL);
        assert(furi_thread_tls_get(&furi) == NULL);
        assert(furi.pointers[0] == &posix_values);
        assert(furi.callbacks[0] == &deletion_callback);
        assert(native.pointers[0] == &posix_values);
        assert(native.callbacks[0] == &deletion_callback);
    }
    puts("PASS: 10000 Furi/ESP-IDF TLS isolation and cleanup cycles");
    return 0;
}
