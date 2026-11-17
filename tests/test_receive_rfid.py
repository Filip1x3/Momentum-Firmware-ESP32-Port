"""Receive-only lifecycle and board-capability wiring regressions.

These are source contracts, not evidence of a physical LF/RF read.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = "applications_user/protopirate/"


def function(path, name):
    source = (ROOT / path).read_text(encoding="utf-8")
    match = re.search(r"^[\w* \t]+\b" + re.escape(name) + r"\([^;{}]*\)\s*\{", source, re.M)
    if not match:
        raise AssertionError(f"Function definition missing: {name}")
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class ReceiveRfidTests(unittest.TestCase):
    def test_stock_board_has_no_125khz_radio_and_override_is_explicit(self):
        board = (ROOT / "components/furi_hal/boards/board_lilygo_t_embed_cc1101.h").read_text()
        self.assertRegex(board, r"#ifndef BOARD_HAS_RFID\s+#define BOARD_HAS_RFID\s+0\s+#endif")
        self.assertIn("BOARD_PIN_RFID_RX", board)  # retain the opt-in external backend
        self.assertIn("return BOARD_HAS_RFID != 0;", function(
            "components/furi_hal/furi_hal_rfid.c", "furi_hal_rfid_is_supported"))

    def test_read_and_raw_read_do_not_start_a_worker_without_hardware(self):
        for scene in ("read", "raw_read"):
            path = f"applications/main/lfrfid/scenes/lfrfid_scene_{scene}.c"
            enter = function(path, f"lfrfid_scene_{scene}_on_enter")
            self.assertLess(enter.index("if(!furi_hal_rfid_is_supported())"),
                            enter.index("lfrfid_worker_start_thread"))
            self.assertIn("lfrfid_show_read_hardware_required(app);\n        return;", enter)
            leave = function(path, f"lfrfid_scene_{scene}_on_exit")
            self.assertLess(leave.index("if(!furi_hal_rfid_is_supported())"),
                            leave.index("lfrfid_worker_stop("))
        alloc = function("applications/main/lfrfid/lfrfid.c", "lfrfid_alloc")
        self.assertIn("calloc(1, sizeof(LfRfid))", alloc)

    def test_decoder_worker_never_queues_gui_events_or_nests_view_operations(self):
        cb = function(APP + "scenes/protopirate_scene_receiver.c",
                      "protopirate_scene_receiver_callback")
        self.assertNotIn("view_dispatcher_send_custom_event(", cb)
        self.assertNotIn("protopirate_view_receiver_set_", cb)
        self.assertNotIn("protopirate_view_receiver_append_", cb)
        self.assertIn("app->receiver_history_dirty = true;", cb)
        self.assertLess(cb.index("protopirate_view_receiver_lock_history("),
                        cb.index("protopirate_history_add_to_history("))
        self.assertGreater(cb.index("protopirate_view_receiver_unlock_history("),
                           cb.index("protopirate_history_mark_saved_match_pending("))

    def test_worker_is_joined_before_receiver_freed(self):
        body = function(APP + "helpers/protopirate_txrx.c",
                        "protopirate_rx_stack_teardown_for_registry_switch")
        self.assertLess(body.index("subghz_worker_stop("), body.index("subghz_receiver_free("))
        body = function(APP + "helpers/protopirate_txrx.c", "protopirate_rx_end")
        self.assertLess(body.index("subghz_devices_stop_async_rx("),
                        body.index("subghz_worker_stop("))
        body = function(APP + "helpers/protopirate_radio.c", "protopirate_radio_deinit")
        self.assertLess(body.index("subghz_worker_is_running("),
                        body.index("protopirate_radio_free_receiver("))
        self.assertLess(body.index("protopirate_view_receiver_reset_menu("),
                        body.index("protopirate_history_free("))

    def test_live_history_readers_and_deferred_storage_are_serialized(self):
        acquire = function(APP + "views/protopirate_receiver.c",
                           "protopirate_view_receiver_lock_history")
        release = function(APP + "views/protopirate_receiver.c",
                           "protopirate_view_receiver_unlock_history")
        self.assertIn("view_get_model(receiver->view)", acquire)
        self.assertIn("view_commit_model(receiver->view, false)", release)
        event = function(APP + "scenes/protopirate_scene_receiver.c",
                         "protopirate_scene_receiver_on_event")
        tick = event[event.index("event.type == SceneManagerEventTypeTick"):]
        self.assertLess(tick.index("protopirate_view_receiver_lock_history("),
                        tick.index("protopirate_scene_receiver_process_deferred_storage("))
        self.assertLess(tick.index("protopirate_view_receiver_unlock_history("),
                        tick.index("protopirate_scene_receiver_refresh_history("))
        # Existing read-only file-decoder FALs embed their own history code.
        # Changing the opaque allocation layout would silently break their ABI.
        history = (ROOT / APP / "protopirate_history.c").read_text()
        self.assertNotIn("FuriMutex*", history)
        app = (ROOT / APP / "protopirate_app_i.h").read_text()
        self.assertIn('"RX dirty flag must use existing padding to preserve plugin ABI"', app)


if __name__ == "__main__":
    unittest.main()
