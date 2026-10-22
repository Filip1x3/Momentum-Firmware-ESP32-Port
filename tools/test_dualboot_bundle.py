import hashlib
import struct
import tempfile
import unittest
import runpy
from unittest.mock import patch
from pathlib import Path
from dualboot_bundle import inspect_image, check_write_ranges, read_csv, read_table, EXPECTED

def image(project="furi_esp32", version="2.0.0-db1", chip=9):
    header = bytearray(24)
    header[0:2] = bytes([0xE9, 1])
    struct.pack_into("<H", header, 12, chip)
    header[23] = 1
    desc = bytearray(256)
    struct.pack_into("<I", desc, 0, 0xABCD5432)
    desc[16:16 + len(version)] = version.encode()
    desc[48:48 + len(project)] = project.encode()
    data = header + struct.pack("<II", 0x3C000020, len(desc)) + desc
    checksum = 0xEF
    for b in desc: checksum ^= b
    data += b"\0" * (15 - len(data) % 16) + bytes([checksum])
    return bytes(data + hashlib.sha256(data).digest())

class BundleTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.path = Path(self.directory.name) / "app.bin"
    def tearDown(self): self.directory.cleanup()
    def inspect(self, data, project="furi_esp32", **kwargs):
        self.path.write_bytes(data)
        return inspect_image(self.path, project, **kwargs)
    def test_valid_momentum(self): self.assertEqual(self.inspect(image())["project"], "furi_esp32")
    def test_valid_bruce(self): self.inspect(image("bruce_mntm_db1", "1.16-db1"), "bruce_mntm_db1")
    def test_reject_stock(self):
        with self.assertRaises(ValueError): self.inspect(image(version="2.0.0"))
    def test_wrong_project(self):
        with self.assertRaises(ValueError): self.inspect(image("stock-bruce"))
    def test_wrong_chip(self):
        with self.assertRaises(ValueError): self.inspect(image(chip=5))
    def test_oversized(self):
        with self.assertRaises(ValueError): self.inspect(image(), max_size=128)
    def test_truncated(self):
        with self.assertRaises(ValueError): self.inspect(image()[:200])
    def test_bad_hash(self):
        data = bytearray(image()); data[-1] ^= 1
        with self.assertRaises(ValueError): self.inspect(data)
    def test_bad_checksum(self):
        data = bytearray(image()); data[40] ^= 1
        with self.assertRaises(ValueError): self.inspect(data)
    def test_no_digest(self):
        data = bytearray(image()); data[23] = 0
        with self.assertRaises(ValueError): self.inspect(data)
    def test_trailing_data(self):
        with self.assertRaises(ValueError): self.inspect(image() + b"surprise")
    def test_safe_offsets(self):
        check_write_ranges([(0, bytes(0x6000)), (0x8000, bytes(0xC00)),
                            (0x10000, image()), (0x600000, image()), (0xBF0000, bytes(8192))])
    def test_block_nvs_overwrite(self):
        with self.assertRaises(ValueError): check_write_ranges([(0, bytes(0x20000))])
    def test_block_bruce_data_overwrite(self):
        with self.assertRaises(ValueError): check_write_ranges([(0xC12000, bytes(4096))])
    def test_block_overlap(self):
        with self.assertRaises(ValueError): check_write_ranges([(0, bytes(100)), (50, bytes(100))])
    def test_block_flash_overflow(self):
        with self.assertRaises(ValueError): check_write_ranges([(0x1000000, b"x")])
    def test_exact_csv(self):
        self.assertEqual(read_csv(Path(__file__).resolve().parents[1] / "partitions_dualboot_16mb.csv"), EXPECTED)
    def table(self, entries=EXPECTED):
        data = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, address, size, name.encode(), 0)
                        for name, kind, subtype, address, size in entries)
        return data + b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(data).digest()
    def test_binary_table(self):
        self.path.write_bytes(self.table())
        self.assertEqual(read_table(self.path), EXPECTED)
    def test_invalid_table_digest(self):
        data = bytearray(self.table()); data[-1] ^= 1
        self.path.write_bytes(data)
        with self.assertRaises(ValueError): read_table(self.path)
    def test_wrong_nvs_location(self):
        entries = EXPECTED.copy(); entries[0] = ("nvs", 1, 2, 0xC12000, 0x6000)
        self.path.write_bytes(self.table(entries))
        with self.assertRaises(ValueError): read_table(self.path)
    def test_missing_table_digest(self):
        self.path.write_bytes(self.table()[:-32])
        with self.assertRaises(ValueError): read_table(self.path)
    def apps(self, board, enabled):
        with patch.dict("os.environ", {"FLIPPER_BOARD": board, "MOMENTUM_DUALBOOT": enabled}):
            return runpy.run_path(str(Path(__file__).resolve().parents[1] / "fam_config.py"))["APPS"]
    def test_profile_shows_settings(self):
        self.assertIn("dualboot_settings", self.apps("lilygo_t_embed_cc1101", "ON"))
    def test_normal_profile_unchanged(self):
        self.assertNotIn("dualboot_settings", self.apps("lilygo_t_embed_cc1101", "OFF"))
    def test_no_other_boards(self):
        self.assertNotIn("dualboot_settings", self.apps("waveshare_c6", "ON"))

if __name__ == "__main__": unittest.main()
