#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DRAM_ATTR
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define ESP_OK 0
#define ESP_FAIL -1
#define RECORD_STORAGE "storage"
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 0x11
#define ESP_PARTITION_SUBTYPE_DATA_NVS 2
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 0x82
#define ESP_PARTITION_SUBTYPE_DATA_OTA 0
#define ESP_IMAGE_HEADER_MAGIC 0xe9
#define ESP_CHIP_ID_ESP32S3 9
typedef uint32_t StackType_t;
typedef struct { int count; } StaticSemaphore_t;
typedef StaticSemaphore_t* SemaphoreHandle_t;
typedef void (*TaskFunction_t)(void*);
typedef struct { TaskFunction_t function; void* context; } StaticTask_t;
typedef StaticTask_t* TaskHandle_t;
typedef int BaseType_t;
typedef struct Storage { int unused; } Storage;
typedef struct { uint32_t address, size; const char* label; } esp_partition_t;
typedef struct { char project_name[32]; } esp_app_desc_t;
typedef struct { uint8_t magic; uint16_t chip_id; } esp_image_header_t;
const esp_partition_t* esp_ota_get_running_partition(void);
const esp_partition_t* esp_partition_find_first(int type, int subtype, const char* label);
int esp_ota_get_partition_description(const esp_partition_t* p, esp_app_desc_t* desc);
int esp_partition_read(const esp_partition_t* p, size_t offset, void* data, size_t size);
int esp_ota_set_boot_partition(const esp_partition_t* p);
void esp_restart(void);
void* furi_record_open(const char* name);
void furi_record_close(const char* name);
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage);
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t* storage);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, uint32_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
TaskHandle_t xTaskCreateStatic(TaskFunction_t function, const char* name, uint32_t bytes,
                              void* context, unsigned priority, StackType_t* stack, StaticTask_t* task);
BaseType_t xTaskCreate(TaskFunction_t function, const char* name, uint32_t bytes,
                      void* context, unsigned priority, TaskHandle_t* task);
uint32_t ulTaskNotifyTake(BaseType_t clear, uint32_t wait);
void xTaskNotifyGive(TaskHandle_t task);
