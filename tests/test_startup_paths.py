"""Source wiring regressions; these do not replace a physical USB/NFC test."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(path: str, name: str) -> str:
    source = (ROOT / path).read_text(encoding="utf-8")
    # Definitions here contain no brace literals in comments/strings.
    start = source.index("{", source.index(name + "("))
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class StartupWiringTests(unittest.TestCase):
    def test_subghz_startup_readers_are_buffered(self):
        for path, name in [
            ("lib/subghz/subghz_setting.c", "subghz_setting_load"),
            ("lib/subghz/subghz_keystore.c", "subghz_keystore_load"),
            ("applications/main/subghz/subghz_last_settings.c", "subghz_last_settings_load"),
        ]:
            with self.subTest(name=name):
                code = function(path, name)
                self.assertIn("flipper_format_buffered_file_alloc(storage)", code)
                self.assertIn("flipper_format_buffered_file_open_existing(", code)
                self.assertNotIn("flipper_format_file_open_existing(", code)
                self.assertNotIn("flipper_format_file_close(", code)

    def test_nfc_scan_and_read_do_not_load_parser_cache(self):
        for scene in ("detect", "read"):
            code = function(f"applications/main/nfc/scenes/nfc_scene_{scene}.c",
                            f"nfc_scene_{scene}_on_enter")
            self.assertNotIn("nfc_supported_cards_load_cache(", code)
            self.assertNotIn("nfc_show_loading_popup(", code)

    def test_nfc_result_parsing_loads_cache_on_demand(self):
        path = "applications/main/nfc/helpers/nfc_supported_cards.c"
        for name in ("nfc_supported_cards_parse", "nfc_supported_cards_read"):
            code = function(path, name)
            self.assertLess(code.index("nfc_supported_cards_load_cache(instance)"),
                            code.index("instance->load_state !="))
        alloc = function(path, "nfc_supported_cards_alloc")
        self.assertIn("calloc(1, sizeof(NfcSupportedCards))", alloc)
        load = function(path, "nfc_supported_cards_load_cache")
        self.assertLess(load.index("NfcSupportedCardsLoadStateSuccess"),
                        load.index("nfc_supported_cards_load_context_alloc()"))

    def test_usb_request_is_serialized_not_timer_polled(self):
        code = function("applications/services/desktop/desktop.c", "desktop_api_open_usb_storage")
        self.assertIn("view_dispatcher_send_custom_event(", code)
        self.assertNotIn("furi_timer_start(", code)
        self.assertNotIn("open_usb_storage_after_app =", code)

    def test_usb_uses_foreground_not_desktop_view(self):
        code = function("applications/services/desktop/scenes/desktop_scene_usb_storage.c",
                        "desktop_scene_usb_storage_on_enter")
        self.assertIn("view_holder_attach_to_gui(desktop->usb_storage_holder, desktop->gui)", code)
        self.assertNotIn("DesktopViewIdUsbStorage", code)
        self.assertLess(code.index("view_holder_set_view("), code.index("desktop_usb_storage_enter("))
        self.assertLess(code.index("loader_lock("), code.index("desktop_usb_storage_enter("))

    def test_usb_handoff_keeps_card_initialized(self):
        code = function("applications/services/desktop/scenes/desktop_scene_usb_storage.c",
                        "desktop_usb_storage_enter")
        self.assertIn("if(storage_sd_unmount(storage) != FSE_OK)", code)
        self.assertIn("furi_hal_sd_release_fatfs()", code)
        self.assertLess(code.index("storage_sd_unmount(storage)"),
                        code.index("furi_hal_sd_release_fatfs()"))
        # The FAM backend, not the unused POSIX shim, is active in this build.
        detach = function("applications/services/storage/storage_processing.c",
                          "storage_process_sd_unmount")
        self.assertLess(detach.index("storage_open_files_count(storage)"),
                        detach.index("sd_unmount_card(storage)"))
        backend = function("applications/services/storage/storages/storage_ext.c", "sd_unmount_card")
        self.assertIn("f_mount(0, sd_data->path, 0)", backend)
        self.assertNotIn("furi_hal_sd_unmount()", backend)

    def test_usb_exit_restores_sd_before_animation(self):
        code = function("applications/services/desktop/scenes/desktop_scene_usb_storage.c",
                        "desktop_scene_usb_storage_on_exit")
        self.assertLess(code.index("desktop_usb_storage_leave("),
                        code.index("desktop_resume_after_usb_storage("))
        self.assertLess(code.index("desktop_resume_after_usb_storage("),
                        code.index("view_holder_free("))

    def test_usb_stops_animation_before_detaching_sd(self):
        code = function("applications/services/desktop/desktop.c", "desktop_suspend_for_usb_storage")
        self.assertIn("desktop_auto_lock_inhibit(desktop)", code)
        self.assertIn("animation_manager_unload_and_stall_animation(", code)


if __name__ == "__main__":
    unittest.main()
