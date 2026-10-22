#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { FSAM_READ, FSAM_READ_WRITE } FS_AccessMode;
typedef enum { FSOM_OPEN_EXISTING } FS_OpenMode;
typedef enum { FSE_OK, FSE_NOT_EXIST } FS_Error;
typedef struct {
    const uint8_t* data;
    size_t size;
    size_t reads;
    size_t writes;
    size_t seeks;
    size_t max_read;
    bool missing;
} Storage;
