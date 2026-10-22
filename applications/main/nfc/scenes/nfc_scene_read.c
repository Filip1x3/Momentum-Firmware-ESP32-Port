#include "../helpers/protocol_support/nfc_protocol_support.h"
#include "../nfc_app_i.h"

void nfc_scene_read_on_enter(void* context) {
    NfcApp* instance = context;
    /* Reading uses the built-in protocol support, not the SD parser plugins.
     * Leave parser loading until a result actually needs to be displayed. */

    nfc_protocol_support_on_enter(NfcProtocolSupportSceneRead, instance);
}

bool nfc_scene_read_on_event(void* context, SceneManagerEvent event) {
    return nfc_protocol_support_on_event(NfcProtocolSupportSceneRead, context, event);
}

void nfc_scene_read_on_exit(void* context) {
    nfc_protocol_support_on_exit(NfcProtocolSupportSceneRead, context);
}
