#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define furi_assert(condition) assert(condition)
#define furi_check(condition) assert(condition)
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_E(...) ((void)0)
#define FuriWaitForever UINT32_MAX
enum {FuriStatusOk};
void* furi_record_open(const char* name);
void furi_record_close(const char* name);
void furi_delay_ms(uint32_t delay);
int furi_message_queue_put(void* queue, const void* data, uint32_t timeout);
typedef struct {bool unlocked;} TestApiLock;
typedef TestApiLock* FuriApiLock;
FuriApiLock api_lock_alloc_locked(void);
void api_lock_wait_unlock_and_free(FuriApiLock lock);
