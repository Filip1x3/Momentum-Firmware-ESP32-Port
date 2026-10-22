#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <dialogs/dialogs.h>
#include <fw_ota/fw_dualboot.h>

typedef struct {
    ViewDispatcher* dispatcher;
    DialogsApp* dialogs;
} DualBootApp;

static uint32_t exit_app(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void start_bruce(void* context, uint32_t index) {
    UNUSED(index);
    DualBootApp* app = context;
    char error[128];
    if(!fw_dualboot_check(error, sizeof(error))) {
        dialog_message_show_storage_error(app->dialogs, error);
        return;
    }
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, "Start Bruce?", 64, 1, AlignCenter, AlignTop);
    dialog_message_set_text(message, "SD files: /bruce", 64, 28,
                            AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "Cancel", "Start", NULL);
    DialogMessageButton choice = dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
    if(choice == DialogMessageButtonCenter && !fw_dualboot_start_bruce(error, sizeof(error))) {
        dialog_message_show_storage_error(app->dialogs, error);
    }
}

int32_t dualboot_settings_app(void* context) {
    UNUSED(context);
    Gui* gui = furi_record_open(RECORD_GUI);
    DualBootApp app = {.dispatcher = view_dispatcher_alloc(),
                       .dialogs = furi_record_open(RECORD_DIALOGS)};
    Submenu* submenu = submenu_alloc();
    submenu_set_header(submenu, "Dual Boot:");
    submenu_add_item(submenu, "Bruce", 0, start_bruce, &app);
    view_set_previous_callback(submenu_get_view(submenu), exit_app);
    view_dispatcher_add_view(app.dispatcher, 0, submenu_get_view(submenu));
    view_dispatcher_attach_to_gui(app.dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app.dispatcher, 0);
    view_dispatcher_run(app.dispatcher);
    view_dispatcher_remove_view(app.dispatcher, 0);
    submenu_free(submenu);
    view_dispatcher_free(app.dispatcher);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    return 0;
}
