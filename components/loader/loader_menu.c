#include <gui.h>
#include <view_dispatcher.h>
#include <menu.h>
#include <submenu.h>
#include <variable_item_list.h>
#include <assets_icons.h>
#include <applications.h>
#include <dialogs/dialogs.h>
#include <flipper_application/flipper_application.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <toolbox/stream/file_stream.h>
#include <archive/helpers/archive_favorites.h>
#include <esp_rom_sys.h>

#include "loader.h"
#include "loader_menu.h"
#include "menu_custom.h"

/* Desktop settings mirror: layout-compatible with
 * applications/services/desktop/desktop_settings.h so we can call the
 * desktop_settings_load/save/services-api functions linked into the final
 * image without pulling the FAM include path into this component. */
typedef struct {
    char name_or_path[128];
} LoaderMenuFavoriteApp;

typedef struct {
    uint32_t auto_lock_delay_ms;
    uint8_t usb_inhibit_auto_lock;
    uint8_t displayBatteryPercentage;
    uint8_t dummy_mode;
    uint8_t display_clock;
    LoaderMenuFavoriteApp favorite_apps[5];
    LoaderMenuFavoriteApp dummy_apps[9];
} LoaderMenuDesktopSettings;

typedef struct LoaderMenuDesktop LoaderMenuDesktop;
typedef struct LoaderMenuPower LoaderMenuPower;

#define LOADER_DISPLAY_BATTERY_OFF              6
#define LOADER_DISPLAY_BATTERY_BAR              0
#define LOADER_DISPLAY_BATTERY_PERCENT          1
#define LOADER_DISPLAY_BATTERY_INVERTED_PERCENT 2
#define LOADER_DISPLAY_BATTERY_BAR_PERCENT      5

void desktop_settings_load(LoaderMenuDesktopSettings* settings);
void desktop_settings_save(const LoaderMenuDesktopSettings* settings);
void desktop_api_get_settings(LoaderMenuDesktop* instance, LoaderMenuDesktopSettings* settings);
void desktop_api_set_settings(
    LoaderMenuDesktop* instance,
    const LoaderMenuDesktopSettings* settings);
void power_trigger_ui_update(LoaderMenuPower* power);

#define LOADER_RECORD_DESKTOP "desktop"
#define LOADER_RECORD_POWER "power"

#define TAG "LoaderMenu"

#define LOADER_MENU_STACK_SIZE (8192)

#define LOADER_MENU_MAX_ENTRIES (96)
#define LOADER_MENU_MAX_FAPS MENU_CUSTOM_MAX_ITEMS

struct LoaderMenu {
    FuriThread* thread;
    void (*closed_cb)(void*);
    void* context;
    bool settings_first;
};

static void loader_menu_trace(const char* step) {
    esp_rom_printf(
        "\r\n[LM] %s free=%u\r\n",
        step,
        (unsigned)furi_thread_get_stack_space(furi_thread_get_current_id()));
}

static int32_t loader_menu_thread(void* p);

LoaderMenu* loader_menu_alloc(void (*closed_cb)(void*), void* context) {
    LoaderMenu* loader_menu = malloc(sizeof(LoaderMenu));
    loader_menu->closed_cb = closed_cb;
    loader_menu->context = context;
    loader_menu->settings_first = false;
    loader_menu->thread =
        furi_thread_alloc_ex(TAG, LOADER_MENU_STACK_SIZE, loader_menu_thread, loader_menu);
    furi_thread_start(loader_menu->thread);
    return loader_menu;
}

LoaderMenu* loader_menu_alloc_settings_first(void (*closed_cb)(void*), void* context) {
    LoaderMenu* loader_menu = malloc(sizeof(LoaderMenu));
    loader_menu->closed_cb = closed_cb;
    loader_menu->context = context;
    loader_menu->settings_first = true;
    loader_menu->thread =
        furi_thread_alloc_ex(TAG, LOADER_MENU_STACK_SIZE, loader_menu_thread, loader_menu);
    furi_thread_start(loader_menu->thread);
    return loader_menu;
}

void loader_menu_free(LoaderMenu* loader_menu) {
    furi_assert(loader_menu);
    furi_thread_join(loader_menu->thread);
    furi_thread_free(loader_menu->thread);
    free(loader_menu);
}

typedef enum {
    LoaderMenuViewPrimary,
    LoaderMenuViewSettings,
    LoaderMenuViewMomentum,
    LoaderMenuViewInterface,
    LoaderMenuViewProtocols,
    LoaderMenuViewMisc,
    LoaderMenuViewInterfaceMain,
    LoaderMenuViewInterfaceLock,
    LoaderMenuViewInterfaceStatus,
    LoaderMenuViewAdd,
    LoaderMenuViewAddMain,
    LoaderMenuViewRemove,
} LoaderMenuView;

typedef enum {
    LoaderMenuEntryKindApplications, // "Apps" (FAP browser)
    LoaderMenuEntryKindInternal, // FLIPPER_APPS[i]
    LoaderMenuEntryKindExternal, // FLIPPER_EXTERNAL_APPS[i]
    LoaderMenuEntryKindInternalAdded, // custom added internal app (FLIPPER_SYSTEM_APPS[i])
    LoaderMenuEntryKindFap, // custom added .fap
    LoaderMenuEntryKindMomentum, // built-in Momentum settings
    LoaderMenuEntryKindSettings, // switch to settings submenu
} LoaderMenuEntryKind;

typedef struct {
    LoaderMenuEntryKind kind;
    uint32_t index; // array index of the source app / fap slot
} LoaderMenuEntry;

/* Runtime Icon, layout-compatible with gui/icon_i.h Icon (fields are const there,
 * so we build it through this writable mirror and cast when adding to the menu). */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t frame_count;
    uint8_t frame_rate;
    const uint8_t* const* frames;
} LoaderMenuRuntimeIcon;

typedef struct {
    FuriString* path; // full .fap path
    FuriString* name; // display name (from manifest or fallback)
    uint8_t icon_data[FAP_MANIFEST_MAX_ICON_SIZE]; // manifest icon incl. flag byte
    const uint8_t* frames[1]; // -> icon_data
    LoaderMenuRuntimeIcon icon; // {10,10,1,0,frames}
} LoaderMenuFapSlot;

typedef struct {
    Gui* gui;
    Storage* storage;
    DialogsApp* dialogs;
    ViewDispatcher* view_dispatcher;
    Menu* primary_menu;
    Submenu* settings_menu;
    Submenu* momentum_menu;
    Submenu* interface_menu;
    Submenu* protocols_menu;
    Submenu* misc_menu;
    VariableItemList* interface_main_list;
    VariableItemList* interface_lock_list;
    VariableItemList* interface_status_list;
    Submenu* add_menu;
    Submenu* add_main_menu;
    Submenu* remove_menu;
    FuriString* browser_path;

    MenuCustom custom;
    LoaderMenuEntry entries[LOADER_MENU_MAX_ENTRIES];
    size_t entries_count;
    LoaderMenuFapSlot faps[LOADER_MENU_MAX_FAPS];
} LoaderMenuApp;

static void loader_menu_start(const char* name) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_start_with_gui_error(loader, name, NULL);
    furi_record_close(RECORD_LOADER);
}

static void loader_menu_switch_to_view(LoaderMenuApp* app, LoaderMenuView view) {
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static void loader_menu_build_remove(LoaderMenuApp* app);
static void loader_menu_build_add_menu(LoaderMenuApp* app);
static void loader_menu_build_add_main(LoaderMenuApp* app);

/* ---------------------------------------------------------------- */
/* Primary menu (entries table + dispatch callback)                  */
/* ---------------------------------------------------------------- */

static void loader_menu_primary_callback(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    furi_assert(index < app->entries_count);
    const LoaderMenuEntry* entry = &app->entries[index];

    switch(entry->kind) {
    case LoaderMenuEntryKindApplications:
        loader_menu_start(LOADER_APPLICATIONS_NAME);
        break;
    case LoaderMenuEntryKindInternal:
        loader_menu_start(FLIPPER_APPS[entry->index].name);
        break;
    case LoaderMenuEntryKindExternal:
        loader_menu_start(FLIPPER_EXTERNAL_APPS[entry->index].path);
        break;
    case LoaderMenuEntryKindInternalAdded:
        loader_menu_start(FLIPPER_SYSTEM_APPS[entry->index].name);
        break;
    case LoaderMenuEntryKindFap:
        loader_menu_start(furi_string_get_cstr(app->faps[entry->index].path));
        break;
    case LoaderMenuEntryKindMomentum:
        loader_menu_start("Momentum");
        break;
    case LoaderMenuEntryKindSettings:
        loader_menu_switch_to_view(app, LoaderMenuViewSettings);
        break;
    }
}

static size_t loader_menu_entries_add(LoaderMenuApp* app, LoaderMenuEntryKind kind, uint32_t index) {
    furi_assert(app->entries_count < LOADER_MENU_MAX_ENTRIES);
    LoaderMenuEntry* entry = &app->entries[app->entries_count];
    entry->kind = kind;
    entry->index = index;
    return app->entries_count++;
}

static const char* loader_menu_entries_label(const LoaderMenuApp* app, size_t pos) {
    const LoaderMenuEntry* entry = &app->entries[pos];
    switch(entry->kind) {
    case LoaderMenuEntryKindApplications:
        return LOADER_APPLICATIONS_NAME;
    case LoaderMenuEntryKindInternal:
        return FLIPPER_APPS[entry->index].name;
    case LoaderMenuEntryKindExternal:
        return FLIPPER_EXTERNAL_APPS[entry->index].name;
    case LoaderMenuEntryKindInternalAdded:
        return FLIPPER_SYSTEM_APPS[entry->index].name;
    case LoaderMenuEntryKindFap:
        return furi_string_get_cstr(app->faps[entry->index].name);
    case LoaderMenuEntryKindMomentum:
        return "Momentum";
    case LoaderMenuEntryKindSettings:
        return "Settings";
    }
    return "";
}

static const Icon* loader_menu_entries_icon(const LoaderMenuApp* app, size_t pos) {
    const LoaderMenuEntry* entry = &app->entries[pos];
    switch(entry->kind) {
    case LoaderMenuEntryKindApplications:
        return &A_Plugins_14;
    case LoaderMenuEntryKindInternal:
        return FLIPPER_APPS[entry->index].icon;
    case LoaderMenuEntryKindExternal:
        return FLIPPER_EXTERNAL_APPS[entry->index].icon;
    case LoaderMenuEntryKindInternalAdded:
        return FLIPPER_SYSTEM_APPS[entry->index].icon;
    case LoaderMenuEntryKindFap:
        return (const Icon*)&app->faps[entry->index].icon;
    case LoaderMenuEntryKindMomentum:
        return &A_Momentum_14;
    case LoaderMenuEntryKindSettings:
        return &A_Settings_14;
    }
    return NULL;
}

/* Load a .fap's display name and 10x10 icon into a persistent slot. */
static bool loader_menu_fap_slot_load(LoaderMenuApp* app, size_t slot_idx, const char* path) {
    LoaderMenuFapSlot* slot = &app->faps[slot_idx];
    furi_string_set(slot->path, path);

    uint8_t* icon_ptr = slot->icon_data;
    furi_string_reset(slot->name);
    bool ok = flipper_application_load_name_and_icon(slot->path, app->storage, &icon_ptr, slot->name);

    if(!ok || furi_string_empty(slot->name)) {
        path_extract_filename(slot->path, slot->name, true);
    }
    if(!ok) {
        memset(slot->icon_data, 0, sizeof(slot->icon_data));
    }

    slot->frames[0] = slot->icon_data;
    slot->icon.width = 10;
    slot->icon.height = 10;
    slot->icon.frame_count = 1;
    slot->icon.frame_rate = 0;
    slot->icon.frames = slot->frames;
    return ok;
}

static int32_t loader_menu_system_app_index_by_appid(const char* appid) {
    for(size_t i = 0; i < FLIPPER_SYSTEM_APPS_COUNT; ++i) {
        if(FLIPPER_SYSTEM_APPS[i].appid && (strcmp(FLIPPER_SYSTEM_APPS[i].appid, appid) == 0)) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Is this appid currently visible in the primary menu? */
static bool loader_menu_app_is_visible(const LoaderMenuApp* app, const char* appid) {
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; ++i) {
        if(FLIPPER_APPS[i].appid && (strcmp(FLIPPER_APPS[i].appid, appid) == 0)) {
            return !menu_custom_is_hidden(&app->custom, appid);
        }
    }
    for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT; ++i) {
        if(FLIPPER_EXTERNAL_APPS[i].path && (strcmp(FLIPPER_EXTERNAL_APPS[i].path, appid) == 0)) {
            return !menu_custom_is_hidden(&app->custom, appid);
        }
    }
    return menu_custom_is_internal_added(&app->custom, appid);
}

/* Is this appid shown as a default (non-hidden) menu entry, ignoring added_internal? */
static bool loader_menu_is_default_visible(const LoaderMenuApp* app, const char* appid) {
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; ++i) {
        if(FLIPPER_APPS[i].appid && (strcmp(FLIPPER_APPS[i].appid, appid) == 0)) {
            return !menu_custom_is_hidden(&app->custom, appid);
        }
    }
    for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT; ++i) {
        if(FLIPPER_EXTERNAL_APPS[i].path && (strcmp(FLIPPER_EXTERNAL_APPS[i].path, appid) == 0)) {
            return !menu_custom_is_hidden(&app->custom, appid);
        }
    }
    return false;
}

/* Is this appid one of the default-menu external apps (unhide instead of add)? */
static bool loader_menu_is_default_external(const char* appid) {
    for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT; ++i) {
        if(FLIPPER_EXTERNAL_APPS[i].path && (strcmp(FLIPPER_EXTERNAL_APPS[i].path, appid) == 0)) {
            return true;
        }
    }
    return false;
}

static bool loader_menu_add_momentum_entry(
    LoaderMenuApp* app,
    const FuriString* value,
    size_t* fap_slot) {
    const char* name_or_path = furi_string_get_cstr(value);

    if(name_or_path[0] == '/') {
        if(*fap_slot >= LOADER_MENU_MAX_FAPS) return false;
        loader_menu_fap_slot_load(app, *fap_slot, name_or_path);
        loader_menu_entries_add(app, LoaderMenuEntryKindFap, (uint32_t)*fap_slot);
        (*fap_slot)++;
        return true;
    }

    if(strcmp(name_or_path, "Bad USB") == 0 || strcmp(name_or_path, "Bad Keyboard") == 0 ||
       strcmp(name_or_path, "bad_kb") == 0) {
        name_or_path = "Bad KB";
    }
    for(size_t i = 0; i < FLIPPER_APPS_COUNT; ++i) {
        if(strcmp(name_or_path, FLIPPER_APPS[i].name) == 0) {
            loader_menu_entries_add(app, LoaderMenuEntryKindInternal, (uint32_t)i);
            return true;
        }
    }
    for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT; ++i) {
        if(strcmp(name_or_path, FLIPPER_EXTERNAL_APPS[i].name) == 0 ||
           (FLIPPER_EXTERNAL_APPS[i].path &&
            strcmp(name_or_path, FLIPPER_EXTERNAL_APPS[i].path) == 0)) {
            loader_menu_entries_add(app, LoaderMenuEntryKindExternal, (uint32_t)i);
            return true;
        }
    }
    return false;
}

static bool loader_menu_build_from_momentum_file(LoaderMenuApp* app) {
    Stream* stream = file_stream_alloc(app->storage);
    FuriString* line = furi_string_alloc();
    uint32_t version = 0;
    bool valid = file_stream_open(stream, MAINMENU_APPS_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
                 stream_read_line(stream, line) &&
                 sscanf(furi_string_get_cstr(line), "MenuAppList Version %lu", &version) == 1 &&
                 version <= 1;

    if(valid) {
        loader_menu_entries_add(app, LoaderMenuEntryKindApplications, 0);
        size_t fap_slot = 0;
        while(stream_read_line(stream, line)) {
            furi_string_trim(line);
            if(furi_string_empty(line)) continue;
            if(version == 0) {
                if(furi_string_equal(line, "RFID")) {
                    furi_string_set(line, "125 kHz RFID");
                } else if(furi_string_equal(line, "SubGHz")) {
                    furi_string_set(line, "Sub-GHz");
                } else if(furi_string_equal(line, "Xtreme")) {
                    furi_string_set(line, "Momentum");
                }
            }
            loader_menu_add_momentum_entry(app, line, &fap_slot);
        }

        /* Include newly ported modules in older saved lists without rewriting
         * the user's SD menu configuration; explicit hidden entries stay hidden. */
        for(size_t i = 0; i < FLIPPER_APPS_COUNT; ++i) {
            const char* appid = FLIPPER_APPS[i].appid;
            if(!appid || strcmp(appid, "u2f") != 0 ||
               menu_custom_is_hidden(&app->custom, appid)) continue;
            bool already_added = false;
            for(size_t j = 0; j < app->entries_count; ++j) {
                already_added |= app->entries[j].kind == LoaderMenuEntryKindInternal &&
                                 app->entries[j].index == i;
            }
            if(!already_added) loader_menu_entries_add(app, LoaderMenuEntryKindInternal, (uint32_t)i);
        }

        if(FLIPPER_EXTERNAL_APPS_COUNT > 0) {
            const size_t last = FLIPPER_EXTERNAL_APPS_COUNT - 1;
            bool already_added = false;
            for(size_t i = 0; i < app->entries_count; ++i) {
                already_added |= app->entries[i].kind == LoaderMenuEntryKindExternal &&
                                 app->entries[i].index == last;
            }
            if(!already_added) {
                loader_menu_entries_add(app, LoaderMenuEntryKindExternal, (uint32_t)last);
            }
        }
        if((FLIPPER_EXTSETTINGS_APPS_COUNT > 0) || (FLIPPER_SETTINGS_APPS_COUNT > 0)) {
            loader_menu_entries_add(app, LoaderMenuEntryKindSettings, 0);
        }
    }

    file_stream_close(stream);
    furi_string_free(line);
    stream_free(stream);
    return valid;
}

static void loader_menu_build_menu(LoaderMenuApp* app) {
    menu_reset(app->primary_menu);
    app->entries_count = 0;

    if(loader_menu_build_from_momentum_file(app)) goto populate_menu;

    if(!menu_custom_is_hidden(&app->custom, LOADER_APPLICATIONS_NAME)) {
        loader_menu_entries_add(app, LoaderMenuEntryKindApplications, 0);
    }

    for(size_t i = 0; i < FLIPPER_APPS_COUNT; ++i) {
        if(FLIPPER_APPS[i].appid && menu_custom_is_hidden(&app->custom, FLIPPER_APPS[i].appid)) {
            continue;
        }
        loader_menu_entries_add(app, LoaderMenuEntryKindInternal, (uint32_t)i);
    }

    for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT; ++i) {
        if(FLIPPER_EXTERNAL_APPS[i].path &&
           menu_custom_is_hidden(&app->custom, FLIPPER_EXTERNAL_APPS[i].path)) {
            continue;
        }
        loader_menu_entries_add(app, LoaderMenuEntryKindExternal, (uint32_t)i);
    }

    for(size_t i = 0; i < app->custom.added_internal_count; ++i) {
        const char* appid = furi_string_get_cstr(app->custom.added_internal[i]);
        int32_t sys_idx = loader_menu_system_app_index_by_appid(appid);
        if(sys_idx < 0) continue;
        if(loader_menu_app_is_visible(app, appid)) continue; // already shown as default
        loader_menu_entries_add(app, LoaderMenuEntryKindInternalAdded, (uint32_t)sys_idx);
    }

    for(size_t i = 0; i < app->custom.added_fap_count; ++i) {
        const char* path = furi_string_get_cstr(app->custom.added_fap[i]);
        loader_menu_fap_slot_load(app, i, path);
        loader_menu_entries_add(app, LoaderMenuEntryKindFap, (uint32_t)i);
    }

    if((FLIPPER_EXTSETTINGS_APPS_COUNT > 0) || (FLIPPER_SETTINGS_APPS_COUNT > 0)) {
        if(!menu_custom_is_hidden(&app->custom, "Settings")) {
            loader_menu_entries_add(app, LoaderMenuEntryKindSettings, 0);
        }
    }

populate_menu:
    for(size_t i = 0; i < app->entries_count; ++i) {
        menu_add_item(
            app->primary_menu,
            loader_menu_entries_label(app, i),
            loader_menu_entries_icon(app, i),
            (uint32_t)i,
            loader_menu_primary_callback,
            app);
    }
}

/* ---------------------------------------------------------------- */
/* Settings submenu                                                  */
/* ---------------------------------------------------------------- */

static void loader_menu_build_interface(LoaderMenuApp* app);

static void
    loader_menu_settings_menu_callback(void* context, InputType input_type, uint32_t index) {
    UNUSED(context);
    if(input_type == InputTypeShort) {
        loader_menu_start((const char*)index);
    } else if(input_type == InputTypeLong) {
        archive_favorites_handle_setting_pin_unpin((const char*)index, NULL);
    }
}

static void loader_menu_update_fw_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
    /* Firmware-/SD-Update lebt in der WiFi-App; dort in den kombinierten
     * Update-Flow springen (Launch-Arg "update"). Gleiches Muster wie der
     * Web-Filesystem-Start aus dem Desktop-Lock-Menü. */
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_start_detached_with_gui_error(loader, "wlan", "update");
    furi_record_close(RECORD_LOADER);
}

static void loader_menu_momentum_interface_callback(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    loader_menu_build_interface(app);
    loader_menu_switch_to_view(app, LoaderMenuViewInterface);
}

static void loader_menu_momentum_protocols_callback(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    loader_menu_switch_to_view(app, LoaderMenuViewProtocols);
}

static void loader_menu_momentum_misc_callback(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    loader_menu_switch_to_view(app, LoaderMenuViewMisc);
}

enum {
    MomentumProtocolSubGhz,
    MomentumProtocolNfc,
    MomentumProtocolInfrared,
    MomentumProtocolU2f,
    MomentumProtocolBadUsb,
};

static void loader_menu_momentum_protocol_item_callback(void* context, uint32_t index) {
    UNUSED(context);
    static const char* const apps[] = {"Sub-GHz", "NFC", "Infrared", "U2F", "Bad KB"};
    if(index < COUNT_OF(apps)) loader_menu_start(apps[index]);
}

static void loader_menu_build_protocols(LoaderMenuApp* app) {
    submenu_reset(app->protocols_menu);
    submenu_set_header(app->protocols_menu, "Protocols:");
    submenu_add_item(app->protocols_menu, "SubGHz Freqs", MomentumProtocolSubGhz, loader_menu_momentum_protocol_item_callback, app);
    submenu_add_item(app->protocols_menu, "NFC", MomentumProtocolNfc, loader_menu_momentum_protocol_item_callback, app);
    submenu_add_item(app->protocols_menu, "Infrared", MomentumProtocolInfrared, loader_menu_momentum_protocol_item_callback, app);
    submenu_add_item(app->protocols_menu, "U2F Authentication", MomentumProtocolU2f, loader_menu_momentum_protocol_item_callback, app);
    submenu_add_item(app->protocols_menu, "Bad KB", MomentumProtocolBadUsb, loader_menu_momentum_protocol_item_callback, app);
}

enum {
    MomentumMiscScreen,
    MomentumMiscDolphin,
    MomentumMiscSpoof,
    MomentumMiscWifi,
};

static void loader_menu_momentum_misc_item_callback(void* context, uint32_t index) {
    UNUSED(context);
    static const char* const apps[] = {
        "Sound & Display",
        "Passport",
        "Spoofing",
        "WiFi",
    };
    if(index < COUNT_OF(apps)) loader_menu_start(apps[index]);
}

static void loader_menu_build_misc(LoaderMenuApp* app) {
    submenu_reset(app->misc_menu);
    submenu_set_header(app->misc_menu, "Misc:");
    submenu_add_item(app->misc_menu, "Screen", MomentumMiscScreen, loader_menu_momentum_misc_item_callback, app);
    submenu_add_item(app->misc_menu, "Dolphin", MomentumMiscDolphin, loader_menu_momentum_misc_item_callback, app);
    submenu_add_item(app->misc_menu, "Spoofing Options", MomentumMiscSpoof, loader_menu_momentum_misc_item_callback, app);
    submenu_add_item(app->misc_menu, "WiFi / ESP32 Options", MomentumMiscWifi, loader_menu_momentum_misc_item_callback, app);
}

static void loader_menu_build_submenu(LoaderMenuApp* app) {
    static const char* const order[] = {
        "Bluetooth", "LCD and Notifications", "Storage", "Power", "Desktop",
        "Passport", "System", "Clock & Alarm",
#ifdef MOMENTUM_DUALBOOT
        "Dual Boot",
#endif
        "About",
    };
    submenu_reset(app->settings_menu);
    for(size_t position = 0; position < COUNT_OF(order); position++) {
        for(size_t i = 0; i < FLIPPER_SETTINGS_APPS_COUNT; i++) {
            if(strcmp(order[position], FLIPPER_SETTINGS_APPS[i].name) == 0) {
                submenu_add_item_ex(app->settings_menu, order[position],
                    (uint32_t)FLIPPER_SETTINGS_APPS[i].name,
                    loader_menu_settings_menu_callback, app);
            }
        }
        for(size_t i = 0; i < FLIPPER_EXTSETTINGS_APPS_COUNT; i++) {
            if(strcmp(order[position], FLIPPER_EXTSETTINGS_APPS[i].name) == 0) {
                submenu_add_item_ex(app->settings_menu, order[position],
                    (uint32_t)FLIPPER_EXTSETTINGS_APPS[i].path,
                    loader_menu_settings_menu_callback, app);
            }
        }
    }
}

static void loader_menu_build_momentum(LoaderMenuApp* app) {
    submenu_reset(app->momentum_menu);
    submenu_set_header(app->momentum_menu, "Momentum:");
    submenu_add_item(
        app->momentum_menu, "Interface", 0, loader_menu_momentum_interface_callback, app);
    submenu_add_item(
        app->momentum_menu, "Protocols", 1, loader_menu_momentum_protocols_callback, app);
    submenu_add_item(app->momentum_menu, "Misc", 2, loader_menu_momentum_misc_callback, app);
}

/* ---------------------------------------------------------------- */
/* Interface submenu (Add/Remove Item)                               */
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/* Interface submenu (Main menu / Lock screen / Status bar)         */
/* ---------------------------------------------------------------- */

/* Tab callbacks: switch into the desired Interface sub-tab */
static void loader_menu_interface_go_main(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    loader_menu_switch_to_view(app, LoaderMenuViewInterfaceMain);
}

static void loader_menu_interface_go_lock(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    loader_menu_switch_to_view(app, LoaderMenuViewInterfaceLock);
}

static void loader_menu_interface_go_status(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    loader_menu_switch_to_view(app, LoaderMenuViewInterfaceStatus);
}

static void loader_menu_build_interface(LoaderMenuApp* app) {
    submenu_reset(app->interface_menu);
    submenu_set_header(app->interface_menu, "Interface:");
    submenu_add_item(app->interface_menu, "Main Menu", 0, loader_menu_interface_go_main, app);
    submenu_add_item(app->interface_menu, "Lock Screen", 1, loader_menu_interface_go_lock, app);
    submenu_add_item(app->interface_menu, "Status Bar", 2, loader_menu_interface_go_status, app);
}

/* ---------------------------------------------------------------- */
/* Interface -> Main Menu tab (menu style + Add/Remove Item)        */
/* ---------------------------------------------------------------- */

static const char* const loader_menu_menu_style_text[MenuStyleCount] = {
    "List",
    "DSi",
    "Wii",
};

static void loader_menu_main_menu_style_changed(VariableItem* item) {
    uint32_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, loader_menu_menu_style_text[index]);
    menu_set_style((MenuStyle)index);
}

static void loader_menu_main_menu_enter(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    if(index == 1) {
        loader_menu_build_add_menu(app);
        loader_menu_switch_to_view(app, LoaderMenuViewAdd);
    } else if(index == 2) {
        loader_menu_build_remove(app);
        loader_menu_switch_to_view(app, LoaderMenuViewRemove);
    }
}

static void loader_menu_build_interface_main(LoaderMenuApp* app) {
    variable_item_list_reset(app->interface_main_list);
    variable_item_list_set_header(app->interface_main_list, "Main Menu:");

    VariableItem* item = variable_item_list_add(
        app->interface_main_list,
        "Menu style",
        MenuStyleCount,
        loader_menu_main_menu_style_changed,
        app);
    uint32_t menu_style = (uint32_t)menu_get_style();
    if(menu_style >= MenuStyleCount) {
        menu_style = 0;
    }
    variable_item_set_current_value_index(item, (uint8_t)menu_style);
    variable_item_set_current_value_text(item, loader_menu_menu_style_text[menu_style]);

    variable_item_list_add(app->interface_main_list, "Add Item", 0, NULL, app);
    variable_item_list_add(app->interface_main_list, "Remove Item", 0, NULL, app);
    variable_item_list_set_enter_callback(app->interface_main_list, loader_menu_main_menu_enter, app);
}

/* ---------------------------------------------------------------- */
/* Interface -> Lock screen tab                                      */
/* ---------------------------------------------------------------- */

static void loader_menu_build_interface_lock(LoaderMenuApp* app) {
    variable_item_list_reset(app->interface_lock_list);
    variable_item_list_set_header(app->interface_lock_list, "Lock Screen:");

    VariableItem* item = variable_item_list_add(
        app->interface_lock_list, "Style", 1, NULL, app);
    variable_item_set_current_value_text(item, "Momentum");
}

/* ---------------------------------------------------------------- */
/* Interface -> Status bar tab                                       */
/* ---------------------------------------------------------------- */

#define LOADER_BATTERY_VIEW_COUNT 5
#define LOADER_CLOCK_ENABLE_COUNT 2

static const char* loader_menu_battery_view_text[LOADER_BATTERY_VIEW_COUNT] = {
    "OFF",
    "Bar",
    "%",
    "Inv. %",
    "Bar %",
};

static const uint32_t loader_menu_battery_view_value[LOADER_BATTERY_VIEW_COUNT] = {
    LOADER_DISPLAY_BATTERY_OFF,
    LOADER_DISPLAY_BATTERY_BAR,
    LOADER_DISPLAY_BATTERY_PERCENT,
    LOADER_DISPLAY_BATTERY_INVERTED_PERCENT,
    LOADER_DISPLAY_BATTERY_BAR_PERCENT,
};

static const char* loader_menu_clock_enable_text[LOADER_CLOCK_ENABLE_COUNT] = {
    "OFF",
    "ON",
};

static const uint32_t loader_menu_clock_enable_value[LOADER_CLOCK_ENABLE_COUNT] = {0, 1};

static void loader_menu_battery_view_changed(VariableItem* item) {
    uint32_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, loader_menu_battery_view_text[index]);

    LoaderMenuDesktopSettings* settings = malloc(sizeof(LoaderMenuDesktopSettings));
    desktop_settings_load(settings);
    settings->displayBatteryPercentage = (uint8_t)loader_menu_battery_view_value[index];
    desktop_settings_save(settings);
    free(settings);

    if(furi_record_exists(LOADER_RECORD_POWER)) {
        LoaderMenuPower* power = furi_record_open(LOADER_RECORD_POWER);
        power_trigger_ui_update(power);
        furi_record_close(LOADER_RECORD_POWER);
    }
}

static void loader_menu_clock_enabled_changed(VariableItem* item) {
    uint32_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, loader_menu_clock_enable_text[index]);

    if(furi_record_exists(LOADER_RECORD_DESKTOP)) {
        LoaderMenuDesktop* desktop = furi_record_open(LOADER_RECORD_DESKTOP);
        LoaderMenuDesktopSettings* settings = malloc(sizeof(LoaderMenuDesktopSettings));
        desktop_api_get_settings(desktop, settings);
        settings->display_clock = (uint8_t)loader_menu_clock_enable_value[index];
        desktop_api_set_settings(desktop, settings);
        free(settings);
        furi_record_close(LOADER_RECORD_DESKTOP);
    } else {
        LoaderMenuDesktopSettings* settings = malloc(sizeof(LoaderMenuDesktopSettings));
        desktop_settings_load(settings);
        settings->display_clock = (uint8_t)loader_menu_clock_enable_value[index];
        desktop_settings_save(settings);
        free(settings);
    }
}

static void loader_menu_build_interface_status(LoaderMenuApp* app) {
    variable_item_list_reset(app->interface_status_list);
    variable_item_list_set_header(app->interface_status_list, "Status Bar:");

    LoaderMenuDesktopSettings* settings = malloc(sizeof(LoaderMenuDesktopSettings));
    desktop_settings_load(settings);

    VariableItem* item = variable_item_list_add(
        app->interface_status_list,
        "Battery Icon",
        LOADER_BATTERY_VIEW_COUNT,
        loader_menu_battery_view_changed,
        app);
    uint32_t v = 0;
    for(uint32_t i = 0; i < LOADER_BATTERY_VIEW_COUNT; i++) {
        if(loader_menu_battery_view_value[i] == settings->displayBatteryPercentage) {
            v = i;
            break;
        }
    }
    variable_item_set_current_value_index(item, (uint8_t)v);
    variable_item_set_current_value_text(item, loader_menu_battery_view_text[v]);

    item = variable_item_list_add(
        app->interface_status_list,
        "Show Clock",
        LOADER_CLOCK_ENABLE_COUNT,
        loader_menu_clock_enabled_changed,
        app);
    uint32_t c = (settings->display_clock != 0) ? 1 : 0;
    variable_item_set_current_value_index(item, (uint8_t)c);
    variable_item_set_current_value_text(item, loader_menu_clock_enable_text[c]);

    free(settings);
}

/* ---------------------------------------------------------------- */
/* Add Item view                                                     */
/* ---------------------------------------------------------------- */

static void loader_menu_add_main_callback(void* context, uint32_t index);
static void loader_menu_add_external_callback(void* context, uint32_t index);
static void loader_menu_add_menu_main_callback(void* context, uint32_t index);

static void loader_menu_build_add_menu(LoaderMenuApp* app) {
    submenu_reset(app->add_menu);
    submenu_set_header(app->add_menu, "Add Menu Item:");
    submenu_add_item(app->add_menu, "Main App", 0, loader_menu_add_menu_main_callback, app);
    submenu_add_item(app->add_menu, "External App", 1, loader_menu_add_external_callback, app);
}

/* "Main App" entry: switch to the built-in apps picker (Add Main view). */
static void loader_menu_add_menu_main_callback(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    loader_menu_build_add_main(app);
    loader_menu_switch_to_view(app, LoaderMenuViewAddMain);
}

static bool loader_menu_add_main_skip(const char* appid) {
    static const char* const blocked[] = {
        "example_apps_assets",
        "example_apps_data",
        "example_number_input",
        "about",
        "js_app",
        "passport",
        "bt_settings",
    };
    for(size_t i = 0; i < COUNT_OF(blocked); ++i) {
        if(strcmp(appid, blocked[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* "Main App" tab: every built-in internal app, locked if already in menu. */
static void loader_menu_build_add_main(LoaderMenuApp* app) {
    submenu_reset(app->add_main_menu);
    submenu_set_header(app->add_main_menu, "Main App");

    for(size_t i = 0; i < FLIPPER_APPS_COUNT; ++i) {
        if(loader_menu_add_main_skip(FLIPPER_APPS[i].appid)) {
            continue;
        }
        bool locked = loader_menu_app_is_visible(app, FLIPPER_APPS[i].appid);
        submenu_add_lockable_item(
            app->add_main_menu,
            FLIPPER_APPS[i].name,
            (uint32_t)i,
            loader_menu_add_main_callback,
            app,
            locked,
            "Already in menu");
    }
    for(size_t i = 0; i < FLIPPER_SYSTEM_APPS_COUNT; ++i) {
        if(loader_menu_add_main_skip(FLIPPER_SYSTEM_APPS[i].appid)) {
            continue;
        }
        bool locked = loader_menu_app_is_visible(app, FLIPPER_SYSTEM_APPS[i].appid);
        submenu_add_lockable_item(
            app->add_main_menu,
            FLIPPER_SYSTEM_APPS[i].name,
            (uint32_t)(FLIPPER_APPS_COUNT + i),
            loader_menu_add_main_callback,
            app,
            locked,
            "Already in menu");
    }
}

static void loader_menu_add_main_callback(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    const char* appid = NULL;
    bool is_default = false;

    if(index < FLIPPER_APPS_COUNT) {
        appid = FLIPPER_APPS[index].appid;
        is_default = true;
    } else {
        size_t sys_idx = index - FLIPPER_APPS_COUNT;
        if(sys_idx < FLIPPER_SYSTEM_APPS_COUNT) {
            appid = FLIPPER_SYSTEM_APPS[sys_idx].appid;
            is_default = loader_menu_is_default_external(appid);
        }
    }
    if(!appid) return;

    bool changed;
    if(is_default) {
        changed = menu_custom_unhide(&app->custom, appid); // bring back a hidden default app
    } else {
        changed = menu_custom_add_internal(&app->custom, appid);
    }
    if(!changed) return;

    menu_custom_save(&app->custom);
    loader_menu_build_menu(app);
    loader_menu_build_add_main(app);
    loader_menu_build_remove(app);
    loader_menu_switch_to_view(app, LoaderMenuViewPrimary);
}

static bool loader_menu_file_browser_item_callback(
    FuriString* path,
    void* context,
    uint8_t** icon,
    FuriString* item_name) {
    LoaderMenuApp* app = context;
    if(furi_string_end_with(path, ".fap")) {
        return flipper_application_load_name_and_icon(path, app->storage, icon, item_name);
    }
    return false;
}

static void loader_menu_add_external_callback(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;

    const DialogsFileBrowserOptions browser_options = {
        .extension = ".fap",
        .skip_assets = true,
        .icon = &I_unknown_10px,
        .hide_ext = true,
        .item_loader_callback = loader_menu_file_browser_item_callback,
        .item_loader_context = app,
        .base_path = EXT_PATH("apps"),
    };

    furi_string_set(app->browser_path, EXT_PATH("apps"));
    if(!dialog_file_browser_show(app->dialogs, app->browser_path, app->browser_path, &browser_options)) {
        return; // cancelled, stay in Add view
    }
    if(!furi_string_end_with(app->browser_path, ".fap")) {
        return;
    }

    if(!menu_custom_add_fap(&app->custom, furi_string_get_cstr(app->browser_path))) {
        return; // already added
    }

    menu_custom_save(&app->custom);
    loader_menu_build_menu(app);
    loader_menu_build_remove(app);
    loader_menu_switch_to_view(app, LoaderMenuViewPrimary);
}

/* ---------------------------------------------------------------- */
/* Remove Item view                                                  */
/* ---------------------------------------------------------------- */

static void loader_menu_remove_callback(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    furi_assert(index < app->entries_count);
    const LoaderMenuEntry* entry = &app->entries[index];

    bool changed = false;
    switch(entry->kind) {
    case LoaderMenuEntryKindApplications:
        changed = menu_custom_hide(&app->custom, LOADER_APPLICATIONS_NAME);
        break;
    case LoaderMenuEntryKindInternal:
        changed = menu_custom_hide(&app->custom, FLIPPER_APPS[entry->index].appid);
        break;
    case LoaderMenuEntryKindExternal:
        changed = menu_custom_hide(&app->custom, FLIPPER_EXTERNAL_APPS[entry->index].path);
        break;
    case LoaderMenuEntryKindInternalAdded:
        changed = menu_custom_remove_internal(&app->custom, FLIPPER_SYSTEM_APPS[entry->index].appid);
        break;
    case LoaderMenuEntryKindFap:
        changed = menu_custom_remove_fap(&app->custom, furi_string_get_cstr(app->faps[entry->index].path));
        break;
    default:
        break; // Settings is not removable
    }

    if(!changed) return;

    menu_custom_save(&app->custom);
    loader_menu_build_menu(app);
    loader_menu_build_add_main(app);
    loader_menu_build_remove(app); // stay on the Remove view
}

void loader_menu_build_remove(LoaderMenuApp* app) {
    submenu_reset(app->remove_menu);
    submenu_set_header(app->remove_menu, "Remove Item:");
    for(size_t i = 0; i < app->entries_count; ++i) {
        const LoaderMenuEntry* entry = &app->entries[i];
        if(entry->kind == LoaderMenuEntryKindSettings ||
           entry->kind == LoaderMenuEntryKindMomentum) {
            continue;
        }
        submenu_add_item(
            app->remove_menu,
            loader_menu_entries_label(app, i),
            (uint32_t)i,
            loader_menu_remove_callback,
            app);
    }
}

/* ---------------------------------------------------------------- */
/* Back navigation                                                   */
/* ---------------------------------------------------------------- */

static uint32_t loader_menu_switch_to_primary(void* context) {
    UNUSED(context);
    return LoaderMenuViewPrimary;
}

static uint32_t loader_menu_switch_to_settings(void* context) {
    UNUSED(context);
    return LoaderMenuViewSettings;
}

static uint32_t loader_menu_switch_to_momentum(void* context) {
    UNUSED(context);
    return LoaderMenuViewMomentum;
}

static uint32_t loader_menu_switch_to_interface(void* context) {
    UNUSED(context);
    return LoaderMenuViewInterface;
}

static uint32_t loader_menu_switch_to_add(void* context) {
    UNUSED(context);
    return LoaderMenuViewAdd;
}

static uint32_t loader_menu_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/* ---------------------------------------------------------------- */
/* App lifecycle                                                     */
/* ---------------------------------------------------------------- */

static LoaderMenuApp* loader_menu_app_alloc(LoaderMenu* loader_menu) {
    LoaderMenuApp* app = malloc(sizeof(LoaderMenuApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->view_dispatcher = view_dispatcher_alloc();
    app->primary_menu = menu_alloc();
    app->settings_menu = submenu_alloc();
    app->add_menu = submenu_alloc();
    app->add_main_menu = submenu_alloc();
    app->remove_menu = submenu_alloc();
    app->browser_path = furi_string_alloc();

    for(size_t i = 0; i < LOADER_MENU_MAX_FAPS; ++i) {
        app->faps[i].path = furi_string_alloc();
        app->faps[i].name = furi_string_alloc();
    }

    menu_custom_load(&app->custom);
    loader_menu_build_menu(app);
    loader_menu_build_submenu(app);
    loader_menu_build_add_menu(app);
    loader_menu_build_add_main(app);
    loader_menu_build_remove(app);

    // Primary menu
    View* primary_view = menu_get_view(app->primary_menu);
    view_set_context(primary_view, app->primary_menu);
    view_set_previous_callback(primary_view, loader_menu_exit);
    view_dispatcher_add_view(app->view_dispatcher, LoaderMenuViewPrimary, primary_view);

    // Settings menu
    View* settings_view = submenu_get_view(app->settings_menu);
    view_set_context(settings_view, app->settings_menu);
    view_set_previous_callback(settings_view, loader_menu_switch_to_primary);
    view_dispatcher_add_view(app->view_dispatcher, LoaderMenuViewSettings, settings_view);

    // Add menu
    View* add_view = submenu_get_view(app->add_menu);
    view_set_context(add_view, app->add_menu);
    view_set_previous_callback(add_view, loader_menu_switch_to_interface);
    view_dispatcher_add_view(app->view_dispatcher, LoaderMenuViewAdd, add_view);

    // Add Main App menu
    View* add_main_view = submenu_get_view(app->add_main_menu);
    view_set_context(add_main_view, app->add_main_menu);
    view_set_previous_callback(add_main_view, loader_menu_switch_to_add);
    view_dispatcher_add_view(app->view_dispatcher, LoaderMenuViewAddMain, add_main_view);

    // Remove menu
    View* remove_view = submenu_get_view(app->remove_menu);
    view_set_context(remove_view, app->remove_menu);
    view_set_previous_callback(remove_view, loader_menu_switch_to_interface);
    view_dispatcher_add_view(app->view_dispatcher, LoaderMenuViewRemove, remove_view);

    LoaderMenuView initial_view = LoaderMenuViewPrimary;
    if(loader_menu->settings_first) initial_view = LoaderMenuViewSettings;
    view_dispatcher_switch_to_view(app->view_dispatcher, initial_view);

    return app;
}

static void loader_menu_app_free(LoaderMenuApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, LoaderMenuViewPrimary);
    view_dispatcher_remove_view(app->view_dispatcher, LoaderMenuViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, LoaderMenuViewAdd);
    view_dispatcher_remove_view(app->view_dispatcher, LoaderMenuViewAddMain);
    view_dispatcher_remove_view(app->view_dispatcher, LoaderMenuViewRemove);
    view_dispatcher_free(app->view_dispatcher);

    menu_free(app->primary_menu);
    submenu_free(app->settings_menu);
    submenu_free(app->add_menu);
    submenu_free(app->add_main_menu);
    submenu_free(app->remove_menu);

    menu_custom_free(&app->custom);
    for(size_t i = 0; i < LOADER_MENU_MAX_FAPS; ++i) {
        furi_string_free(app->faps[i].path);
        furi_string_free(app->faps[i].name);
    }
    furi_string_free(app->browser_path);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

static int32_t loader_menu_thread(void* p) {
    LoaderMenu* loader_menu = p;
    furi_assert(loader_menu);
    loader_menu_trace("thread_start");

    LoaderMenuApp* app = loader_menu_app_alloc(loader_menu);
    loader_menu_trace("app_alloc");

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    loader_menu_trace("attached");
    view_dispatcher_run(app->view_dispatcher);
    loader_menu_trace("dispatcher_return");

    if(loader_menu->closed_cb) {
        loader_menu->closed_cb(loader_menu->context);
    }

    loader_menu_app_free(app);
    loader_menu_trace("thread_end");

    return 0;
}
