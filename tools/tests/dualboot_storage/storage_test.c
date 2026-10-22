/* Host regression test of the real helper, with recording Storage API stubs.
 * The stub uses the firmware's actual filesystem enum/type definitions. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fw_dualboot_storage.h"

struct Storage { int unused; };
struct File { FS_Error error; bool open; };
enum { Healthy, NoCard, MkdirDenied, FolderExists, FolderIsFile, StatFailed,
       NoMemory, OpenDenied, ShortWrite, SyncFailed, CloseFailed, RemoveFailed };
static int scenario, collisions, opens, closes, frees, removes, root_stats;
static struct File file;
static char created_path[40];

FS_Error storage_sd_status(Storage* storage) {
    assert(storage);
    return scenario == NoCard ? FSE_NOT_READY : FSE_OK;
}
FS_Error storage_common_mkdir(Storage* storage, const char* path) {
    assert(storage && !strcmp(path, "/ext/bruce"));
    if(scenario == MkdirDenied) return FSE_DENIED;
    return scenario == FolderExists ? FSE_EXIST : FSE_OK;
}
FS_Error storage_common_stat(Storage* storage, const char* path, FileInfo* info) {
    assert(storage && info);
    if(!strcmp(path, "/ext")) {
        ++root_stats;
        return FSE_INVALID_NAME; /* Real FatFs root-stat behavior. */
    }
    assert(!strcmp(path, "/ext/bruce"));
    info->flags = scenario == FolderIsFile ? 0 : FSF_DIRECTORY;
    return scenario == StatFailed ? FSE_NOT_EXIST : FSE_OK;
}
File* storage_file_alloc(Storage* storage) {
    assert(storage);
    return scenario == NoMemory ? NULL : &file;
}
void storage_file_free(File* value) {
    assert(value == &file && !file.open);
    ++frees;
}
bool storage_file_open(File* value, const char* path, FS_AccessMode access, FS_OpenMode mode) {
    assert(value == &file && !file.open);
    assert(access == FSAM_WRITE && mode == FSOM_CREATE_NEW);
    char expected[40];
    snprintf(expected, sizeof(expected), "/ext/bruce/DBCHK%03u.TMP", (unsigned)opens);
    assert(!strcmp(path, expected));
    ++opens;
    if(opens <= collisions) { file.error = FSE_EXIST; return false; }
    if(scenario == OpenDenied) { file.error = FSE_DENIED; return false; }
    snprintf(created_path, sizeof(created_path), "%s", path);
    file.open = true;
    return true;
}
FS_Error storage_file_get_error(File* value) { assert(value == &file); return file.error; }
size_t storage_file_write(File* value, const void* data, size_t size) {
    assert(value == &file && file.open);
    assert(size == strlen("dualboot-v1\n") && !memcmp(data, "dualboot-v1\n", size));
    return scenario == ShortWrite ? size - 1 : size;
}
bool storage_file_sync(File* value) {
    assert(value == &file && file.open);
    return scenario != SyncFailed;
}
bool storage_file_close(File* value) {
    assert(value == &file);
    bool was_open = file.open;
    file.open = false;
    ++closes;
    return was_open && scenario != CloseFailed;
}
FS_Error storage_common_remove(Storage* storage, const char* path) {
    assert(storage && !file.open && created_path[0] && !strcmp(path, created_path));
    ++removes;
    return scenario == RemoveFailed ? FSE_DENIED : FSE_OK;
}

static void check(int failure, int conflicts, bool expected) {
    struct Storage storage = {0};
    char error[128] = "old error";
    scenario = failure; collisions = conflicts;
    opens = closes = frees = removes = root_stats = 0;
    memset(&file, 0, sizeof(file));
    created_path[0] = '\0';
    assert(fw_dualboot_prepare_sd(&storage, error, sizeof(error)) == expected);
    assert((error[0] == '\0') == expected);
    assert(!root_stats); /* Regression: never stat a FAT volume root. */
    assert(!file.open);
    if(created_path[0]) assert(removes == 1 && frees == 1);
    else assert(removes == 0);
    if(conflicts >= 16) assert(opens == 16 && closes == 16);
}

int main(void) {
    check(Healthy, 0, true);
    check(FolderExists, 0, true);
    check(Healthy, 1, true); /* Existing file untouched; next name is exclusive. */
    check(Healthy, 15, true);
    check(Healthy, 16, false); /* No truncation even when all names collide. */
    check(NoCard, 0, false);
    check(MkdirDenied, 0, false);
    check(FolderIsFile, 0, false);
    check(StatFailed, 0, false);
    check(NoMemory, 0, false);
    check(OpenDenied, 0, false);
    check(ShortWrite, 0, false);
    check(SyncFailed, 0, false);
    check(CloseFailed, 0, false);
    check(RemoveFailed, 0, false);
    puts("15 Dual Boot SD regression scenarios passed");
    return 0;
}
