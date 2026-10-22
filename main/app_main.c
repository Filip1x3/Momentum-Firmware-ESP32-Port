#include <furi.h>
#include <furi_hal.h>
#include <flipper.h>
#include <applications.h>
#include <gui/canvas_i.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <assets_icons.h>
#include <momentum/settings_i.h>
#include <momentum/namespoof.h>
#include <momentum/asset_packs.h>
#include <fw_ota/fw_dualboot.h>

#include <esp_log.h>
#include <esp_rom_uart.h>
#ifdef MOMENTUM_BOOT_DIAGNOSTICS
#include <esp_rom_caps.h>
#include <esp_rom_sys.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#define BOOT_DIAG(stage) esp_rom_printf("[BOOTDIAG] %s internal=%u largest=%u\r\n", stage, \
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT), \
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))
#else
#define BOOT_DIAG(stage) ((void)0)
#endif

static const char* TAG = "Main";

static void momentum_boot_screen_show(void) {
    /* Keep the stock striped Apps glyph on boot. A distinct Icon identity
     * prevents asset-pack Archive/Apps replacements from changing it, while
     * still sharing the original bitmap used by the built-in assets. */
    const Icon boot_apps_icon = I_Apps_10px;
    Gui* gui = furi_record_open(RECORD_GUI);
    Canvas* canvas = gui_direct_draw_acquire(gui);
    canvas_reset(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Direct port of Momentum-Firmware/furi/flipper.c boot drawing. */
    canvas_draw_icon(canvas, 33, 16, &I_Updating_Logo_62x15);
    canvas_draw_icon(canvas, 19, 44, &I_SDcardMounted_11x8);
    canvas_commit(canvas);
    furi_delay_ms(100);
    canvas_draw_icon(canvas, 39, 43, &I_dir_10px);
    canvas_commit(canvas);
    furi_delay_ms(100);
    canvas_draw_icon(canvas, 59, 42, &boot_apps_icon);
    canvas_commit(canvas);
    furi_delay_ms(100);
    canvas_draw_icon(canvas, 79, 44, &I_Rpc_active_7x8);
    canvas_commit(canvas);
    furi_delay_ms(100);
    canvas_draw_icon(canvas, 99, 44, &I_Hidden_window_9x8);
    canvas_commit(canvas);
    furi_delay_ms(500);
    gui_direct_draw_release(gui);
    furi_record_close(RECORD_GUI);
}

static void log_internal_registry(
    const char* label,
    const FlipperInternalApplication* apps,
    size_t count) {
    if(count == 0) {
        ESP_LOGI(TAG, "%s: none", label);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        ESP_LOGI(
            TAG,
            "%s[%u]: appid=%s name=%s stack=%lu",
            label,
            (unsigned)i,
            apps[i].appid ? apps[i].appid : "(null)",
            apps[i].name ? apps[i].name : "(null)",
            (unsigned long)apps[i].stack_size);
    }
}

static void log_external_registry(
    const char* label,
    const FlipperExternalApplication* apps,
    size_t count) {
    if(count == 0) {
        ESP_LOGI(TAG, "%s: none", label);
        return;
    }

    for(size_t i = 0; i < count; ++i) {
        ESP_LOGI(
            TAG,
            "%s[%u]: name=%s launch=%s",
            label,
            (unsigned)i,
            apps[i].name ? apps[i].name : "(null)",
            apps[i].path ? apps[i].path : "(null)");
    }
}

static bool registry_contains_appid(
    const FlipperInternalApplication* apps,
    size_t count,
    const char* appid) {
    for(size_t i = 0; i < count; ++i) {
        if(apps[i].appid && strcmp(apps[i].appid, appid) == 0) {
            return true;
        }
    }

    return false;
}

static bool registry_contains_launch(
    const FlipperExternalApplication* apps,
    size_t count,
    const char* launch) {
    for(size_t i = 0; i < count; ++i) {
        if(apps[i].path && strcmp(apps[i].path, launch) == 0) {
            return true;
        }
    }

    return false;
}

static void log_registry_snapshot(void) {
    ESP_LOGI(
        TAG,
        "Registry counts: services=%u startup=%u apps=%u settings=%u extsettings=%u external=%u internal_external=%u system=%u debug=%u",
        (unsigned)FLIPPER_SERVICES_COUNT,
        (unsigned)FLIPPER_ON_SYSTEM_START_COUNT,
        (unsigned)FLIPPER_APPS_COUNT,
        (unsigned)FLIPPER_SETTINGS_APPS_COUNT,
        (unsigned)FLIPPER_EXTSETTINGS_APPS_COUNT,
        (unsigned)FLIPPER_EXTERNAL_APPS_COUNT,
        (unsigned)FLIPPER_INTERNAL_EXTERNAL_APPS_COUNT,
        (unsigned)FLIPPER_SYSTEM_APPS_COUNT,
        (unsigned)FLIPPER_DEBUG_APPS_COUNT);

    log_internal_registry("service", FLIPPER_SERVICES, FLIPPER_SERVICES_COUNT);
    log_internal_registry("settings", FLIPPER_SETTINGS_APPS, FLIPPER_SETTINGS_APPS_COUNT);
    log_external_registry("extsettings", FLIPPER_EXTSETTINGS_APPS, FLIPPER_EXTSETTINGS_APPS_COUNT);

    if(!registry_contains_appid(FLIPPER_SERVICES, FLIPPER_SERVICES_COUNT, "bt")) {
        ESP_LOGW(TAG, "Bluetooth service appid 'bt' is not present in FLIPPER_SERVICES");
    }

    if(!registry_contains_appid(FLIPPER_SETTINGS_APPS, FLIPPER_SETTINGS_APPS_COUNT, "bt_settings") &&
       !registry_contains_launch(
           FLIPPER_EXTSETTINGS_APPS, FLIPPER_EXTSETTINGS_APPS_COUNT, "bt_settings")) {
        ESP_LOGW(
            TAG,
            "Bluetooth settings app 'bt_settings' is not present in settings or extsettings registry");
    }
}

static void furi_log_esp_callback(const uint8_t* data, size_t size, void* context) {
    (void)context;
    for(size_t i = 0; i < size; ++i) {
        esp_rom_output_putc((char)data[i]);
    }
}

void app_main(void) {
#ifdef MOMENTUM_BOOT_DIAGNOSTICS
    esp_rom_output_set_as_console(ESP_ROM_USB_SERIAL_DEVICE_NUM);
    esp_rom_printf("[BOOTDIAG] reset_reason=%d\r\n", (int)esp_reset_reason());
#endif
    BOOT_DIAG("app_main");
    ESP_LOGI(TAG, "Starting Furi Core on ESP32...");

    // ESP-IDF: Scheduler is already running!
    furi_init();

    // Register ESP32 log handler
    FuriLogHandler log_handler = {
        .callback = furi_log_esp_callback,
        .context = NULL,
    };
    furi_log_add_handler(log_handler);

    furi_hal_init_early();
    furi_hal_init();
    BOOT_DIAG("HAL ready");
    if(!fw_dualboot_init()) {
        ESP_LOGW(TAG, "Dual Boot worker unavailable");
    }
    flipper_init();
    BOOT_DIAG("worker ready");
    log_registry_snapshot();

    bool momentum_initialized = false;
    bool boot_statusbar_hidden = false;
    for(size_t i = 0; i < FLIPPER_SERVICES_COUNT; i++) {
        /* Desktop draws the dolphin and status bar as soon as its thread starts.
         * Initialize Momentum and show its splash immediately before Desktop so
         * no desktop frame can flash underneath the boot animation. */
        if(!momentum_initialized &&
           FLIPPER_SERVICES[i].appid &&
           strcmp(FLIPPER_SERVICES[i].appid, "desktop") == 0) {
            namespoof_init();
            momentum_settings_load();
            BOOT_DIAG("settings loaded");
            if(momentum_settings.dark_mode) {
                uint16_t foreground = furi_hal_display_get_fg_color();
                furi_hal_display_set_fg_color(furi_hal_display_get_bg_color());
                furi_hal_display_set_bg_color(foreground);
            }
            asset_packs_init();
            BOOT_DIAG("assets loaded");
            furi_delay_ms(100);
            momentum_boot_screen_show();
            momentum_initialized = true;
        }

        FuriThread* thread = furi_thread_alloc_service(
            FLIPPER_SERVICES[i].name,
            FLIPPER_SERVICES[i].stack_size,
            FLIPPER_SERVICES[i].app,
            NULL);
        furi_thread_set_appid(thread, FLIPPER_SERVICES[i].appid);
        furi_thread_start(thread);
        BOOT_DIAG(FLIPPER_SERVICES[i].appid);

        /* Hide the GUI status bar before any later service (BT, WiFi, power,
         * storage) can register and draw its icon. Keep it hidden throughout
         * the Momentum splash and reveal it only once Desktop is ready. */
        if(!boot_statusbar_hidden && FLIPPER_SERVICES[i].appid &&
           strcmp(FLIPPER_SERVICES[i].appid, "gui") == 0) {
            Gui* gui = furi_record_open(RECORD_GUI);
            gui_set_hide_statusbar(gui, true);
            furi_record_close(RECORD_GUI);
            boot_statusbar_hidden = true;
        }

        if(boot_statusbar_hidden && FLIPPER_SERVICES[i].appid &&
           strcmp(FLIPPER_SERVICES[i].appid, "desktop") == 0) {
            /* desktop_alloc() needs a moment to attach its main view. Until
             * then the completed splash remains visible without stray icons. */
            furi_delay_ms(100);
            Gui* gui = furi_record_open(RECORD_GUI);
            gui_set_hide_statusbar(gui, false);
            furi_record_close(RECORD_GUI);
            boot_statusbar_hidden = false;
        }

        furi_delay_ms(10);
    }

    /* Fallback for unusual service sets without Desktop. */
    if(!momentum_initialized) {
        namespoof_init();
        momentum_settings_load();
        if(momentum_settings.dark_mode) {
            uint16_t foreground = furi_hal_display_get_fg_color();
            furi_hal_display_set_fg_color(furi_hal_display_get_bg_color());
            furi_hal_display_set_bg_color(foreground);
        }
        asset_packs_init();
        furi_delay_ms(100);
        momentum_boot_screen_show();
    }

    if(boot_statusbar_hidden) {
        Gui* gui = furi_record_open(RECORD_GUI);
        gui_set_hide_statusbar(gui, false);
        furi_record_close(RECORD_GUI);
    }

    for(size_t i = 0; i < FLIPPER_ON_SYSTEM_START_COUNT; i++) {
        FLIPPER_ON_SYSTEM_START[i]();
    }

    BOOT_DIAG("startup hooks complete");
    ESP_LOGI(TAG, "All services started, entering background...");

    // This blocks forever (thread scrubber)
    furi_background();
}
