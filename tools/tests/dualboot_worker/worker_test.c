#include <test_platform.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

/* Include the actual implementation to inspect its static storage in this TU. */
#include "../../../components/fw_ota/fw_dualboot.c"

static jmp_buf worker_yield, reboot_exit;
static bool notify_pending, create_fails, bad_layout, bad_image, sd_ready;
static unsigned static_creates, dynamic_creates, ota_calls, reboot_calls, sd_calls;
static int ota_result = ESP_FAIL;
static Storage storage;
static const esp_partition_t current = {0x10000, 0x5f0000, "ota_0"};
static const esp_partition_t wrong = {0x1000, 0x5f0000, "ota_0"};
static const esp_partition_t partitions[] = {
    {0x600000,0x5f0000,"ota_1"}, {0xc12000,0x10000,"bruce_nvs"},
    {0x9000,0x6000,"nvs"}, {0xc22000,0x3de000,"spiffs"}, {0xbf0000,0x2000,"otadata"},
};
const esp_partition_t* esp_ota_get_running_partition(void) { return bad_layout ? &wrong : &current; }
const esp_partition_t* esp_partition_find_first(int type, int subtype, const char* label) {
    (void)type; (void)subtype;
    for(size_t i=0;i<sizeof(partitions)/sizeof(partitions[0]);++i)
        if(!strcmp(partitions[i].label,label)) return &partitions[i];
    return NULL;
}
int esp_ota_get_partition_description(const esp_partition_t* p, esp_app_desc_t* desc) {
    assert(p == &partitions[0]);
    snprintf(desc->project_name,sizeof(desc->project_name),"%s",
             bad_image ? "stock_bruce" : "bruce_mntm_db1");
    return ESP_OK;
}
int esp_partition_read(const esp_partition_t* p, size_t offset, void* data, size_t size) {
    assert(p == &partitions[0] && !offset && size == sizeof(esp_image_header_t));
    esp_image_header_t header = {ESP_IMAGE_HEADER_MAGIC,ESP_CHIP_ID_ESP32S3};
    memcpy(data,&header,size); return ESP_OK;
}
int esp_ota_set_boot_partition(const esp_partition_t* p) {
    assert(p == &partitions[0]); ota_calls++; return ota_result;
}
void esp_restart(void) { reboot_calls++; longjmp(reboot_exit,1); }
void* furi_record_open(const char* name) { assert(!strcmp(name,RECORD_STORAGE)); return &storage; }
void furi_record_close(const char* name) { assert(!strcmp(name,RECORD_STORAGE)); }
bool fw_dualboot_prepare_sd(Storage* s, char* error, size_t size) {
    assert(s == &storage); sd_calls++;
    if(!sd_ready) snprintf(error,size,"SD card not ready");
    return sd_ready;
}
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* s) { s->count=1; return s; }
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t* s) { s->count=0; return s; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, uint32_t wait) {
    if(s->count) { s->count=0; return pdTRUE; }
    assert(wait == 0); return pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { assert(!s->count); s->count=1; return pdTRUE; }
TaskHandle_t xTaskCreateStatic(TaskFunction_t function, const char* name, uint32_t bytes,
                              void* context, unsigned priority, StackType_t* stack, StaticTask_t* task) {
    assert(!strcmp(name,"dualboot") && bytes == 8192 && priority == 5);
    assert(stack == switch_stack && task == &switch_task_storage);
    static_creates++;
    if(create_fails) return NULL;
    task->function=function; task->context=context; return task;
}
/* Simulate an exhausted dynamic-task heap for the entire test. */
BaseType_t xTaskCreate(TaskFunction_t function, const char* name, uint32_t bytes,
                      void* context, unsigned priority, TaskHandle_t* task) {
    (void)function;(void)name;(void)bytes;(void)context;(void)priority;(void)task;
    dynamic_creates++; return -1;
}
uint32_t ulTaskNotifyTake(BaseType_t clear, uint32_t wait) {
    assert(clear == pdTRUE && wait == portMAX_DELAY);
    if(notify_pending) { notify_pending=false; return 1; }
    longjmp(worker_yield,1);
}
void xTaskNotifyGive(TaskHandle_t task) {
    assert(task == switch_task && !notify_pending);
    notify_pending=true;
    if(!setjmp(worker_yield)) task->function(task->context);
}

int main(void) {
    char error[128];
    assert(!fw_dualboot_start_bruce(error,sizeof(error)) && strstr(error,"unavailable"));
    create_fails=true;
    assert(!fw_dualboot_init());
    assert(!fw_dualboot_start_bruce(error,sizeof(error)) && !ota_calls);
    create_fails=false;
    assert(fw_dualboot_init() && static_creates == 2);
    assert(fw_dualboot_init() && static_creates == 2); /* idempotent */
    bad_layout=true;
    assert(!fw_dualboot_start_bruce(error,sizeof(error)) && strstr(error,"layout"));
    assert(!sd_calls && !ota_calls);
    bad_layout=false; bad_image=true;
    assert(!fw_dualboot_start_bruce(error,sizeof(error)) && strstr(error,"Bruce"));
    assert(!sd_calls && !ota_calls);
    bad_image=false; sd_ready=false;
    assert(!fw_dualboot_start_bruce(error,sizeof(error)) && strstr(error,"SD card"));
    assert(!ota_calls);
    sd_ready=true;
    for(unsigned i=0;i<1000;++i) {
        assert(!fw_dualboot_start_bruce(error,sizeof(error)) && strstr(error,"invalid"));
        assert(switch_lock->count == 1 && switch_done->count == 0);
    }
    assert(ota_calls == 1000 && !dynamic_creates && static_creates == 2);
    assert(xSemaphoreTake(switch_lock,0));
    assert(!fw_dualboot_start_bruce(error,sizeof(error)) && strstr(error,"busy"));
    assert(ota_calls == 1000);
    xSemaphoreGive(switch_lock);
    ota_result=ESP_OK;
    if(!setjmp(reboot_exit)) { fw_dualboot_start_bruce(error,sizeof(error)); assert(false); }
    assert(reboot_calls == 1 && ota_calls == 1001 && !dynamic_creates);
    puts("PASS: reserved static worker, failed creation, idempotent init, wrong layout/image, missing SD, 1000 repeat failures, busy guard and validated reboot; zero dynamic task allocations");
    return 0;
}
