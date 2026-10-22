#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct FuriString FuriString;
#define _ATTRIBUTE(x)
#define furi_check(x) assert(x)
#define furi_assert(x) assert(x)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
