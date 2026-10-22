#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define furi_check(x) assert(x)
#define furi_assert(x) assert(x)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define malloc(size) calloc(1, (size))
typedef struct FuriTimer FuriTimer;
typedef void (*FuriTimerCallback)(void*);
typedef enum { FuriTimerTypePeriodic } FuriTimerType;
typedef enum { FuriStatusOk } FuriStatus;
FuriTimer* furi_timer_alloc(FuriTimerCallback cb, FuriTimerType type, void* context);
void furi_timer_free(FuriTimer* timer);
FuriStatus furi_timer_start(FuriTimer* timer, uint32_t ticks);
FuriStatus furi_timer_stop(FuriTimer* timer);
uint32_t furi_kernel_get_tick_frequency(void);
