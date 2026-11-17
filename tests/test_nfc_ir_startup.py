"""Source-level guards supplement the native fake-platform startup test."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NfcIrStartupTests(unittest.TestCase):
    def test_nfc_cache_lists_without_mapping(self):
        source = (ROOT / "applications/main/nfc/helpers/nfc_supported_cards.c").read_text()
        cache = source.split("void nfc_supported_cards_load_cache(", 1)[1].split(
            "bool nfc_supported_cards_read(", 1)[0]
        self.assertIn("nfc_supported_cards_get_next_name(", cache)
        self.assertNotIn("nfc_supported_cards_get_plugin(", cache)
        self.assertNotIn("flipper_application_map_to_memory(", cache)

    def test_ir_index_does_not_decode_and_send_still_validates(self):
        source = (ROOT / "lib/infrared/signal/infrared_brute_force.c").read_text()
        index = source.split("InfraredErrorCode infrared_brute_force_calculate_messages(", 1)[1].split(
            "bool infrared_brute_force_start(", 1)[0]
        self.assertNotIn("infrared_signal_read_body(", index)
        self.assertIn("SignalPositionArray_reset(", index)
        self.assertIn("if(!record) continue", index)
        start = source.split("bool infrared_brute_force_start(", 1)[1].split(
            "bool infrared_brute_force_is_started(", 1)[0]
        self.assertIn("flipper_format_set_strict_mode(brute_force->ff, true)", start)
        send = source.split("bool infrared_brute_force_send(", 1)[1].split(
            "void infrared_brute_force_add_record(", 1)[0]
        self.assertLess(send.index("infrared_signal_read_body("), send.index("infrared_signal_is_valid("))
        self.assertLess(send.index("infrared_signal_is_valid("), send.index("infrared_signal_transmit("))


if __name__ == "__main__":
    unittest.main()
