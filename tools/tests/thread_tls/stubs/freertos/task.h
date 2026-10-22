#pragma once
typedef struct MockTask* TaskHandle_t;
void* pvTaskGetThreadLocalStoragePointer(TaskHandle_t task, int slot);
void vTaskSetThreadLocalStoragePointer(TaskHandle_t task, int slot, void* value);
