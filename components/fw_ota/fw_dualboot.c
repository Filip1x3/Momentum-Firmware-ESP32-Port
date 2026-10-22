#include "fw_dualboot.h"
#include "fw_dualboot_storage.h"
#include <stdio.h>
#include <string.h>
#include <furi.h>
#include <storage/storage.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static bool fail(char* error, size_t size, const char* message) {
    if(error && size) snprintf(error, size, "%s", message);
    return false;
}

bool fw_dualboot_check(char* error, size_t size) {
#ifndef MOMENTUM_DUALBOOT
    return fail(error, size, "Install the Dual Boot\nfirmware bundle first.");
#else
    const esp_partition_t* current = esp_ota_get_running_partition();
    const esp_partition_t* bruce = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "ota_1");
    const esp_partition_t* nvs = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "bruce_nvs");
    const esp_partition_t* own_nvs = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    const esp_partition_t* fs = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    const esp_partition_t* ota = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, "otadata");
    if(!current || current->address != 0x10000 || current->size != 0x5F0000 ||
       !bruce || bruce->address != 0x600000 || bruce->size != 0x5F0000 ||
       !nvs || nvs->address != 0xC12000 || nvs->size != 0x10000 ||
       !own_nvs || own_nvs->address != 0x9000 || own_nvs->size != 0x6000 ||
       !fs || fs->address != 0xC22000 || fs->size != 0x3DE000 ||
       !ota || ota->address != 0xBF0000 || ota->size != 0x2000) {
        return fail(error, size, "Wrong partition layout.\nInstall the complete\nDual Boot bundle.");
    }
    esp_app_desc_t desc;
    esp_image_header_t header;
    if(esp_ota_get_partition_description(bruce, &desc) != ESP_OK ||
       esp_partition_read(bruce, 0, &header, sizeof(header)) != ESP_OK ||
       header.magic != ESP_IMAGE_HEADER_MAGIC || header.chip_id != ESP_CHIP_ID_ESP32S3 ||
       strncmp(desc.project_name, "bruce_mntm_db1", sizeof(desc.project_name)) != 0) {
        return fail(error, size, "Compatible Bruce image\nnot installed.");
    }
    if(error && size) error[0] = '\0';
    return true;
#endif
}

#ifdef MOMENTUM_DUALBOOT
typedef struct {
    char error[128];
} SwitchRequest;

#define SWITCH_STACK_BYTES 8192u

/* Flash writes require an internal stack. Reserve it before Wi-Fi, graphics
 * and applications can fragment/exhaust the internal heap. The worker remains
 * blocked between requests; its TCB is never deleted/reused while idle cleanup
 * or a previous caller could still reference it. */
static DRAM_ATTR StackType_t switch_stack[SWITCH_STACK_BYTES / sizeof(StackType_t)];
static DRAM_ATTR StaticTask_t switch_task_storage;
static DRAM_ATTR StaticSemaphore_t switch_lock_storage;
static DRAM_ATTR StaticSemaphore_t switch_done_storage;
static DRAM_ATTR SwitchRequest switch_request;
static SemaphoreHandle_t switch_lock;
static SemaphoreHandle_t switch_done;
static TaskHandle_t switch_task;

static void switch_worker(void* context) {
    (void)context;
    for(;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        SwitchRequest* request = &switch_request;
        request->error[0] = '\0';
        if(fw_dualboot_check(request->error, sizeof(request->error))) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            bool ready = fw_dualboot_prepare_sd(storage, request->error, sizeof(request->error));
            furi_record_close(RECORD_STORAGE);
            if(ready) {
                const esp_partition_t* target = esp_partition_find_first(
                    ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "ota_1");
                // IDF validates the complete image before touching the OTA selector.
                if(esp_ota_set_boot_partition(target) == ESP_OK) {
                    esp_restart();
                }
                fail(request->error, sizeof(request->error), "Bruce image invalid or\nboot selection failed.\nMomentum is unchanged.");
            }
        }
        xSemaphoreGive(switch_done);
    }
}
#endif

bool fw_dualboot_init(void) {
#ifndef MOMENTUM_DUALBOOT
    return true;
#else
    if(switch_task) return true;
    switch_lock = xSemaphoreCreateMutexStatic(&switch_lock_storage);
    switch_done = xSemaphoreCreateBinaryStatic(&switch_done_storage);
    if(!switch_lock || !switch_done) return false;
    /* ESP-IDF's stack-depth argument is in BYTES (not vanilla FreeRTOS words). */
    switch_task = xTaskCreateStatic(
        switch_worker, "dualboot", sizeof(switch_stack), NULL, 5,
        switch_stack, &switch_task_storage);
    return switch_task != NULL;
#endif
}

bool fw_dualboot_start_bruce(char* error, size_t size) {
#ifndef MOMENTUM_DUALBOOT
    return fw_dualboot_check(error, size);
#else
    if(!switch_task) return fail(error, size, "Boot service unavailable.\nRestart the device.");
    if(xSemaphoreTake(switch_lock, 0) != pdTRUE) {
        return fail(error, size, "Boot switch is busy.\nPlease wait.");
    }
    xTaskNotifyGive(switch_task);
    xSemaphoreTake(switch_done, portMAX_DELAY);
    fail(error, size, switch_request.error);
    xSemaphoreGive(switch_lock);
    return false;
#endif
}
