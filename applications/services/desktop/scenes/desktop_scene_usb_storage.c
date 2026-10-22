/**
 * USB-Storage scene — exposes the SD card as a USB Mass Storage device.
 *
 * This is the former standalone "USB-Storage" app turned into a desktop scene.
 * It deliberately stays full-screen and blocking (not a background toggle):
 * while active the SD card is detached from the firmware and handed to the
 * host, so no other app could use storage anyway.
 *
 * Mutual exclusion: the qFlipper bridge shares the same TinyUSB composite, so
 * on enter we stop it first.
 *
 * Only the ESP32-S3 / S2 path has USB-OTG; on other targets the scene shows an
 * error.
 */

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>

#include "../desktop_i.h"
#include "../views/desktop_view_usb_storage.h"
#include "../helpers/qflipper_bridge.h"
#include "desktop_scene.h"

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
#include "furi_hal_usb_tinyusb_composite.h"
#include "furi_hal_usb_msc.h"
#include "furi_hal_sd.h"
#define USB_STORAGE_HAVE_USB 1
#else
#define USB_STORAGE_HAVE_USB 0
#endif

#define TAG "DesktopUsbStorage"

/* Whether the SD was mounted before we took it over — restored on leave.
 * Single-instance scene, so a file-static is fine. */
static bool s_sd_was_mounted = false;
static bool s_msc_started = false;
static bool s_composite_installed = false;

void desktop_scene_usb_storage_callback(DesktopEvent event, void* context) {
    Desktop* desktop = (Desktop*)context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, event);
}

#if USB_STORAGE_HAVE_USB
static bool desktop_usb_storage_enter(Desktop* desktop, const char** error_msg) {
    Storage* storage = desktop->storage;

    /* The active FAM Storage service checks open handles and soft-unmounts
     * FATFS. Do not proceed if a background reader still owns an open file. */
    if(storage_sd_status(storage) != FSE_OK) {
        *error_msg = "Insert SD card first";
        return false;
    }
    if(storage_sd_unmount(storage) != FSE_OK) {
        *error_msg = "Close SD files first";
        return false;
    }
    s_sd_was_mounted = true;

    /* Keep the initialized card/SPI device for MSC and synchronize HAL state
     * with the filesystem detach done by the Storage service. */
    if(!furi_hal_sd_release_fatfs()) {
        *error_msg = "FATFS release failed";
        return false;
    }

    /* 3) Ensure the composite (HID + CDC + MSC) is installed. */
    if(!furi_hal_usb_composite_install(0, 0, NULL, NULL)) {
        *error_msg = "USB composite install failed";
        return false;
    }
    s_composite_installed = true;

    /* 4) Flip MSC active — host sees a removable disk. */
    if(!furi_hal_usb_msc_start()) {
        *error_msg = "MSC start failed";
        return false;
    }

    s_msc_started = true;

    return true;
}

static void desktop_usb_storage_leave(Desktop* desktop) {
    /* stop() arms the SCSI sense to MEDIUM_NOT_PRESENT; wait long enough for
     * the host's ~1 s TEST UNIT READY poll so it unmounts before we re-attach
     * the SD on the firmware side. */
    if(s_msc_started) {
        furi_hal_usb_msc_stop();
        furi_delay_ms(1500);
        s_msc_started = false;
    }

    if(s_sd_was_mounted) {
        FS_Error err = storage_sd_mount(desktop->storage);
        if(err != FSE_OK) {
            FURI_LOG_E(TAG, "storage_sd_mount after MSC failed: %d", err);
        }
        s_sd_was_mounted = false;
    }

    /* Tear the shared composite down so the internal USB PHY routes back to
     * USB-Serial-JTAG — same "leave -> flash" restore as the qFlipper toggle. */
    if(s_composite_installed) {
        furi_hal_usb_composite_uninstall();
        s_composite_installed = false;
    }
}
#endif /* USB_STORAGE_HAVE_USB */

void desktop_scene_usb_storage_on_enter(void* context) {
    Desktop* desktop = (Desktop*)context;

    /* USB Storage owns the whole display, just like the Momentum intro. */
    gui_set_hide_statusbar(desktop->gui, true);

    desktop_usb_storage_set_callback(
        desktop->usb_storage_view, desktop_scene_usb_storage_callback, desktop);

    /* A Desktop-layer view is hidden by Loader's still-open Settings menu.
     * Give USB Storage its own foreground layer; input goes to this view only. */
    desktop_suspend_for_usb_storage(desktop);
    desktop->usb_storage_holder = view_holder_alloc();
    view_holder_attach_to_gui(desktop->usb_storage_holder, desktop->gui);
    desktop_usb_storage_set_state(desktop->usb_storage_view, DesktopUsbStorageStateInit, NULL);
    view_holder_set_view(
        desktop->usb_storage_holder, desktop_usb_storage_get_view(desktop->usb_storage_view));

    /* Prevent RPC/the menu from starting another app while SD belongs to USB. */
    desktop->usb_storage_loader_locked = loader_lock(desktop->loader);
    if(!desktop->usb_storage_loader_locked) {
        desktop_usb_storage_set_state(
            desktop->usb_storage_view, DesktopUsbStorageStateError, "Close other app first");
        return;
    }

#if USB_STORAGE_HAVE_USB
    /* Mutual exclusion: the qFlipper bridge holds the shared composite. */
    qflipper_bridge_stop();

    const char* error_msg = NULL;
    if(desktop_usb_storage_enter(desktop, &error_msg)) {
        desktop_usb_storage_set_state(
            desktop->usb_storage_view, DesktopUsbStorageStateActive, NULL);
    } else {
        desktop_usb_storage_set_state(
            desktop->usb_storage_view, DesktopUsbStorageStateError, error_msg);
    }
#else
    desktop_usb_storage_set_state(
        desktop->usb_storage_view,
        DesktopUsbStorageStateError,
        "USB-Storage nicht\nverfügbar auf\ndiesem Board");
#endif
}

bool desktop_scene_usb_storage_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = (Desktop*)context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopUsbStorageEventExit) {
            scene_manager_search_and_switch_to_previous_scene(
                desktop->scene_manager, DesktopSceneMain);
            consumed = true;
        }
    }

    return consumed;
}

void desktop_scene_usb_storage_on_exit(void* context) {
    Desktop* desktop = (Desktop*)context;
#if USB_STORAGE_HAVE_USB
    if(desktop->usb_storage_loader_locked) desktop_usb_storage_leave(desktop);
#endif
    if(desktop->usb_storage_loader_locked) {
        loader_unlock(desktop->loader);
        desktop->usb_storage_loader_locked = false;
    }
    /* Restore SD-based animation before revealing the underlying screen. */
    desktop_resume_after_usb_storage(desktop);
    view_holder_set_view(desktop->usb_storage_holder, NULL);
    view_holder_free(desktop->usb_storage_holder);
    desktop->usb_storage_holder = NULL;
    gui_set_hide_statusbar(desktop->gui, false);
}
