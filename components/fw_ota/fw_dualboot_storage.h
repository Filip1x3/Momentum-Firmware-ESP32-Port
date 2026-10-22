#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <storage/storage.h>

/* Check the card through real directory/file operations, not stat(SD root). */
bool fw_dualboot_prepare_sd(Storage* storage, char* error, size_t size);
