#include "fw_dualboot_storage.h"
#include <stdio.h>

static bool sd_error(char* error, size_t size, const char* message) {
    if(error && size) snprintf(error, size, "%s", message);
    return false;
}

bool fw_dualboot_prepare_sd(Storage* storage, char* error, size_t size) {
    if(storage_sd_status(storage) != FSE_OK) {
        return sd_error(error, size, "SD card is not mounted.\nMount it in Storage\nand try again.");
    }

    /* FatFs f_stat rejects the volume root (FR_INVALID_NAME). A healthy
     * /ext therefore cannot be checked as an ordinary file/directory.
     * Validate our subdirectory and do a real write instead. */
    FS_Error result = storage_common_mkdir(storage, "/ext/bruce");
    if(result != FSE_OK && result != FSE_EXIST) {
        return sd_error(error, size, "Cannot create /bruce.\nCheck SD card and\nfree space.");
    }
    FileInfo info;
    if(storage_common_stat(storage, "/ext/bruce", &info) != FSE_OK ||
       !(info.flags & FSF_DIRECTORY)) {
        return sd_error(error, size, "/bruce is not a folder\nor cannot be read.\nCheck the SD card.");
    }

    File* probe = storage_file_alloc(storage);
    if(!probe) return sd_error(error, size, "Not enough memory\nto check the SD card.");
    bool opened = false;
    char path[40];
    /* Exclusive creation never truncates an existing user/stale test file.
     * Short 8.3 names also work without long-filename support. */
    for(unsigned int index = 0; index < 16; ++index) {
        snprintf(path, sizeof(path), "/ext/bruce/DBCHK%03u.TMP", index);
        opened = storage_file_open(probe, path, FSAM_WRITE, FSOM_CREATE_NEW);
        if(opened) break;
        result = storage_file_get_error(probe);
        storage_file_close(probe);
        if(result != FSE_EXIST) break;
    }
    if(!opened) {
        storage_file_free(probe);
        return sd_error(error, size, "Cannot write to SD.\nCheck card/free space\nand try again.");
    }

    const char marker[] = "dualboot-v1\n";
    bool written = storage_file_write(probe, marker, sizeof(marker) - 1) == sizeof(marker) - 1;
    bool synced = written && storage_file_sync(probe);
    bool closed = storage_file_close(probe);
    storage_file_free(probe);
    /* Remove only the file this invocation successfully created. */
    result = storage_common_remove(storage, path);
    if(!written || !synced || !closed) {
        return sd_error(error, size, "SD write/sync failed.\nCheck card/free space\nand try again.");
    }
    if(result != FSE_OK) {
        return sd_error(error, size, "SD cleanup failed.\nCheck the SD card\nand try again.");
    }
    if(error && size) error[0] = '\0';
    return true;
}
