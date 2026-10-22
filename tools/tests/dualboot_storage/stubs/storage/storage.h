#pragma once
#include <stddef.h>
#include <filesystem_api_defines.h>

typedef struct Storage Storage;
FS_Error storage_sd_status(Storage* storage);
FS_Error storage_common_mkdir(Storage* storage, const char* path);
FS_Error storage_common_stat(Storage* storage, const char* path, FileInfo* info);
FS_Error storage_common_remove(Storage* storage, const char* path);
File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);
bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode);
FS_Error storage_file_get_error(File* file);
size_t storage_file_write(File* file, const void* data, size_t size);
bool storage_file_sync(File* file);
bool storage_file_close(File* file);
