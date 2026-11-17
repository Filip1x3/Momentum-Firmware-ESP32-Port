"""Native memory-only tests using the actual production beacon template. No RF."""
import argparse
import ctypes as C
import random
import re
import unittest
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--library", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / "components/wifi/wlan_hal.c").read_text(encoding="utf-8")
literal = source.split("static const uint8_t beacon_packet_template[] = {", 1)[1].split("};", 1)[0]
template = bytes(int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", literal))
library = C.CDLL(str(args.library.resolve()))
edit = library.wlan_beacon_frame_set_ssid
edit.argtypes = [C.POINTER(C.c_uint8), C.POINTER(C.c_size_t), C.c_size_t, C.c_char_p, C.c_uint8]
edit.restype = C.c_bool


def elements(frame):
    result = []
    offset = 36
    while offset < len(frame):
        assert offset + 2 <= len(frame)
        size = frame[offset + 1]
        assert offset + 2 + size <= len(frame)
        result.append((frame[offset], frame[offset + 2:offset + 2 + size]))
        offset += size + 2
    return result


def apply(frame, name=b"Lab", channel=6, capacity=None):
    capacity = len(frame) if capacity is None else capacity
    storage = max(len(frame), capacity)
    buffer = (C.c_uint8 * (storage + 32))(*([0xA5] * (storage + 32)))
    for index, byte in enumerate(frame):
        buffer[index + 16] = byte
    length = C.c_size_t(len(frame))
    result = edit(C.cast(C.byref(buffer, 16), C.POINTER(C.c_uint8)), C.byref(length), capacity, name, channel)
    assert bytes(buffer[:16]) == b"\xa5" * 16
    assert bytes(buffer[-16:]) == b"\xa5" * 16
    return result, bytes(buffer[16:16 + length.value]), length.value


class BeaconFrameTests(unittest.TestCase):
    def test_real_template_and_all_builtin_names(self):
        original = elements(template)
        self.assertEqual(original[0], (0, b" " * 32))
        self.assertEqual([kind for kind, _ in original], [0, 1, 3, 0x30])
        for part in ("beacon_funny_ssids", "beacon_rickroll_ssids"):
            names = source.split(f"static const char* {part}[] = {{", 1)[1].split("};", 1)[0]
            for name in re.findall(r'"([^"\n]+)"', names):
                ok, frame, size = apply(template, name.encode())
                self.assertTrue(ok, name)
                ies = elements(frame)
                self.assertEqual(ies[0], (0, name.encode()[:32]))
                self.assertEqual(ies[1], original[1])
                self.assertEqual(ies[2], (3, b"\x06"))
                self.assertEqual(ies[3], original[3])
                self.assertEqual(size, len(template) - 32 + min(len(name), 32))

    def test_boundaries_and_repeated_resize(self):
        frame = template
        for _ in range(100):
            for length in (0, 1, 15, 30, 31, 32, 33, 100):
                ok, frame, _ = apply(frame, b"a" * length, channel=11, capacity=len(template))
                self.assertTrue(ok)
                self.assertEqual(elements(frame)[0], (0, b"a" * min(length, 32)))
                self.assertEqual(elements(frame)[2], (3, b"\x0b"))

    def test_rejects_invalid_tlvs_without_mutation(self):
        bad = [template[:35], template[:-1], bytes(40), template[:37] + b"\xff" + template[38:],
               template + bytes((0, 0)), template + bytes((3, 1, 2))]
        for frame in bad:
            ok, actual, size = apply(frame)
            self.assertFalse(ok)
            self.assertEqual(actual, frame)
            self.assertEqual(size, len(frame))
        for channel in (0, 15, 255):
            ok, actual, _ = apply(template, channel=channel)
            self.assertFalse(ok)
            self.assertEqual(actual, template)
        shortened = apply(template, b"x")[1]
        ok, actual, _ = apply(shortened, b"n" * 32, capacity=len(shortened))
        self.assertFalse(ok)
        self.assertEqual(actual, shortened)

    def test_seeded_malformed_inputs_and_no_rf_changes(self):
        rng = random.Random(9178)
        for _ in range(5000):
            frame = rng.randbytes(rng.randrange(0, 180))
            ok, result, _ = apply(frame, b"unit-test", channel=rng.randrange(0, 256))
            if ok:
                elements(result)
            else:
                self.assertEqual(result, frame)
        self.assertIn("packet_size, false)", source)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(10))", source)


if __name__ == "__main__":
    unittest.main(argv=[__file__], verbosity=2)
