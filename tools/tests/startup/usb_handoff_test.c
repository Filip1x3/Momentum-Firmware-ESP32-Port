#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum { FSE_OK, FSE_NOT_READY, FSE_DENIED, FSE_INTERNAL } FS_Error;
typedef enum { StorageStatusOK, StorageStatusNotReady } StorageStatus;
typedef int SDError;
enum { FR_OK, FR_DISK_ERR, ST_EXT = 0 };
typedef struct { void* fs; const char* path; } SDData;
typedef struct { void* data; StorageStatus status; unsigned open_files; } StorageData;
typedef struct { StorageData storage[1]; } Storage;
typedef struct { Storage* storage; } Desktop;

#define FURI_LOG_E(...) ((void)0)
static bool s_sd_was_mounted, s_msc_started, s_composite_installed;
static bool card_initialized, fatfs_mounted, hal_mounted, composite_active, msc_active;
static bool fail_unmount, fail_release, fail_install, fail_start, fail_mount;
static unsigned mounts, unmounts, installs, stops, delays, events, step;
static unsigned stop_step, mount_step, uninstall_step;

static StorageStatus storage_data_status(StorageData* s) { return s->status; }
static unsigned storage_open_files_count(StorageData* s) { return s->open_files; }
static void storage_data_timestamp(StorageData* s) { (void)s; ++events; }
static FS_Error storage_ext_parse_error(SDError error) {
    return error == FR_OK ? FSE_OK : FSE_INTERNAL;
}
static SDError f_mount(void* fs, const char* path, int mode) {
    (void)path; (void)mode;
    assert(!fs && !msc_active);
    ++unmounts;
    if(fail_unmount) return FR_DISK_ERR;
    fatfs_mounted = false;
    return FR_OK;
}
static FS_Error storage_process_sd_unmount(Storage* app);
static FS_Error storage_sd_unmount(Storage* s) { return storage_process_sd_unmount(s); }
static FS_Error storage_sd_status(Storage* s) {
    return s->storage[0].status == StorageStatusOK ? FSE_OK : FSE_NOT_READY;
}
static bool furi_hal_sd_release_fatfs(void) {
    if(fail_release) return false;
    hal_mounted = false;
    return true;
}
static bool furi_hal_usb_composite_install(int vid, int pid, void* a, void* b) {
    (void)vid; (void)pid; (void)a; (void)b;
    ++installs;
    if(fail_install) return false;
    composite_active = true;
    return true;
}
static bool furi_hal_usb_msc_start(void) {
    assert(card_initialized && !fatfs_mounted && !hal_mounted && composite_active);
    if(fail_start) return false;
    msc_active = true;
    return true;
}
static void furi_hal_usb_msc_stop(void) {
    assert(msc_active);
    msc_active = false;
    ++stops;
    stop_step = ++step;
}
static void furi_delay_ms(unsigned ms) { assert(ms == 1500); ++delays; ++step; }
static FS_Error storage_sd_mount(Storage* s) {
    assert(!msc_active && card_initialized);
    ++mounts;
    mount_step = ++step;
    if(fail_mount) return FSE_INTERNAL;
    fatfs_mounted = hal_mounted = true;
    s->storage[0].status = StorageStatusOK;
    return FSE_OK;
}
static void furi_hal_usb_composite_uninstall(void) {
    assert(composite_active && !msc_active);
    composite_active = false;
    uninstall_step = ++step;
}

#include "usb_backend_under_test.h"

static void reset(Desktop* desktop) {
    assert(!s_sd_was_mounted && !s_msc_started && !s_composite_installed);
    *desktop->storage = (Storage){.storage = {{.status = StorageStatusOK}}};
    static SDData data = {.path = "0:"};
    desktop->storage->storage[0].data = &data;
    card_initialized = fatfs_mounted = hal_mounted = true;
    composite_active = msc_active = false;
    fail_unmount = fail_release = fail_install = fail_start = fail_mount = false;
    mounts = unmounts = installs = stops = delays = events = step = 0;
    stop_step = mount_step = uninstall_step = 0;
}

int main(void) {
    Storage storage;
    Desktop desktop = {.storage = &storage};
    const char* error = NULL;
    for(unsigned n = 0; n < 1000; ++n) {
        reset(&desktop);
        assert(desktop_usb_storage_enter(&desktop, &error));
        assert(storage_sd_status(&storage) == FSE_NOT_READY && msc_active);
        desktop_usb_storage_leave(&desktop);
        assert(storage_sd_status(&storage) == FSE_OK && card_initialized);
        assert(!msc_active && !composite_active && fatfs_mounted && hal_mounted);
        assert(unmounts == 1 && mounts == 1 && delays == 1 && stops == 1);
        assert(stop_step < mount_step && mount_step < uninstall_step);
    }
    puts("PASS: 1000 SD -> MSC -> SD handoffs; stop/grace/remount/USB cleanup order");

    reset(&desktop);
    storage.storage[0].open_files = 1;
    assert(!desktop_usb_storage_enter(&desktop, &error) && error);
    desktop_usb_storage_leave(&desktop);
    assert(fatfs_mounted && hal_mounted && unmounts == 0 && installs == 0);

    reset(&desktop);
    storage.storage[0].status = StorageStatusNotReady;
    assert(!desktop_usb_storage_enter(&desktop, &error));
    desktop_usb_storage_leave(&desktop);
    assert(unmounts == 0 && mounts == 0 && installs == 0);

    reset(&desktop);
    fail_unmount = true;
    assert(!desktop_usb_storage_enter(&desktop, &error));
    desktop_usb_storage_leave(&desktop);
    assert(storage_sd_status(&storage) == FSE_OK && fatfs_mounted && installs == 0 && mounts == 0);

    reset(&desktop);
    fail_release = true;
    assert(!desktop_usb_storage_enter(&desktop, &error));
    desktop_usb_storage_leave(&desktop);
    assert(mounts == 1 && installs == 0 && !msc_active && delays == 0);

    reset(&desktop);
    fail_install = true;
    assert(!desktop_usb_storage_enter(&desktop, &error));
    desktop_usb_storage_leave(&desktop);
    assert(mounts == 1 && stops == 0 && delays == 0 && uninstall_step == 0);

    reset(&desktop);
    fail_start = true;
    assert(!desktop_usb_storage_enter(&desktop, &error));
    desktop_usb_storage_leave(&desktop);
    assert(mounts == 1 && stops == 0 && delays == 0 && uninstall_step > mount_step);

    reset(&desktop);
    assert(desktop_usb_storage_enter(&desktop, &error));
    fail_mount = true;
    desktop_usb_storage_leave(&desktop);
    assert(!msc_active && !composite_active && storage_sd_status(&storage) == FSE_NOT_READY);
    puts("PASS: missing SD, open files, unmount/release/install/MSC/remount failures");
    return 0;
}
