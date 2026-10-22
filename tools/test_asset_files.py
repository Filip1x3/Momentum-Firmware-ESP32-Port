"""Host regression tests for the production external-asset parser.

Pass --library path/to/asset_file.dll (or .so) built from asset_file.c and the
bundled heatshrink_decoder.c. Optional --pack ZIP tests original packs in place;
it never extracts, changes, installs or flashes their contents.
"""
import argparse
import ctypes as C
import random
import struct
import unittest
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--library", type=Path, required=True)
parser.add_argument("--pack", type=Path, action="append", default=[])
args = parser.parse_args()
lib = C.CDLL(str(args.library.resolve()))
U8 = C.POINTER(C.c_uint8)
lib.asset_file_decode_bitmap.argtypes = [U8, C.c_size_t, C.c_uint32, C.c_uint32, U8, C.c_size_t]
lib.asset_file_decode_bitmap.restype = C.c_bool
lib.asset_file_validate_font.argtypes = [U8, C.c_size_t]
lib.asset_file_validate_font.restype = C.c_bool
lib.asset_file_fit_dimensions.argtypes = [C.c_uint32] * 4 + [C.POINTER(C.c_uint16)] * 2
lib.asset_file_scale_bitmap.argtypes = [U8, C.c_uint32, C.c_uint32, U8, C.c_uint32, C.c_uint32]
lib.asset_file_scale_bitmap.restype = C.c_bool
lib.asset_file_valid_name.argtypes = [C.c_char_p]
lib.asset_file_valid_name.restype = C.c_bool


def buffer(data):
    return (C.c_uint8 * max(1, len(data)))(*data)


def decode(data, w, h, capacity=1024):
    # Guard both ends, including rejected input and deliberately small capacity.
    out = (C.c_uint8 * (capacity + 32))(*([0xA5] * (capacity + 32)))
    ptr = C.cast(C.byref(out, 16), U8)
    ok = lib.asset_file_decode_bitmap(buffer(data), len(data), w, h, ptr, capacity)
    assert bytes(out[:16]) == b'\xA5' * 16
    assert bytes(out[-16:]) == b'\xA5' * 16
    return ok, bytes(out[16:-16])


def literal_frame(raw):
    bits = ''.join('1' + format(x, '08b') for x in raw)
    bits += '0' * (-len(bits) % 8)
    payload = bytes(int(bits[i:i+8], 2) for i in range(0, len(bits), 8))
    return b'\x01\x00' + struct.pack('<H', len(payload)) + payload


def font(data):
    return lib.asset_file_validate_font(buffer(data), len(data))


class AssetFileTests(unittest.TestCase):
    def test_raw_and_compressed(self):
        raw = bytes(range(128)) * 8
        for data in (b'\0' + raw, literal_frame(raw)):
            ok, decoded = decode(data, 128, 64)
            self.assertTrue(ok)
            self.assertEqual(decoded, raw)

    def test_headers_lengths_and_expansion(self):
        good = b'\0' + b'\xff' * 8
        for data in (b'', b'\1', b'\1\0', b'\1\0\xff\xff', b'\2' + good,
                     good[:-1], good + b'\0', literal_frame(b'\xff' * 9),
                     literal_frame(b'\xff' * 7), b'\1\1\0\0'):
            self.assertFalse(decode(data, 8, 8)[0], data)

    def test_dimensions_and_small_output(self):
        for w, h in [(0, 1), (1, 0), (129, 64), (128, 65), (65535, 65535), (2**32-1, 1)]:
            self.assertFalse(decode(b'\0\0', w, h)[0])
        self.assertFalse(decode(b'\0' + bytes(8), 8, 8, 7)[0])

    def test_padding_for_non_byte_width(self):
        self.assertTrue(decode(b'\0' + bytes(20), 10, 10)[0])
        self.assertFalse(decode(b'\0' + bytes(13), 10, 10)[0])

    def test_fit_and_scaling(self):
        for w, h, mw, mh, expected in [(128,64,14,14,(14,7)), (8,8,8,8,(8,8)),
                                      (2,64,10,8,(1,8)), (8,16,14,14,(7,14))]:
            ow, oh = C.c_uint16(), C.c_uint16()
            lib.asset_file_fit_dimensions(w,h,mw,mh,C.byref(ow),C.byref(oh))
            self.assertEqual((ow.value,oh.value),expected)
        out = buffer(bytes(8))
        self.assertTrue(lib.asset_file_scale_bitmap(buffer(b'\xff'*32),16,16,out,8,8))
        self.assertEqual(bytes(out),b'\xff'*8)

    def test_names(self):
        for name in (b'Haunter', b'Watch Dogs', b'Haunter_Beam', b'swirl'):
            self.assertTrue(lib.asset_file_valid_name(name))
        for name in (b'', b'.', b'..', b'../other', b'a\\b', b'C:x', b'x'*64, b'bad\nname'):
            self.assertFalse(lib.asset_file_valid_name(name))

    def test_font_bounds(self):
        header = bytearray(23)
        header[2:11] = bytes([1]*9)
        header[21:23] = struct.pack('>H', 5)
        good = bytes(header) + b'A\3\0\0\0'
        self.assertTrue(font(good))
        self.assertFalse(font(good[:-1]))
        bad = bytearray(good); bad[17:19] = b'\xff\xff'
        self.assertFalse(font(bad))
        bad = bytearray(good); bad[4] = 0
        self.assertFalse(font(bad))
        bad = bytearray(good); bad[24] = 255
        self.assertFalse(font(bad))

    def test_seeded_malformed_inputs(self):
        rng = random.Random(1101)
        for _ in range(5000):
            data = rng.randbytes(rng.randrange(0, 1400))
            decode(data, rng.randrange(0, 140), rng.randrange(0, 70))
            font(data)

    def test_original_packs(self):
        for path in args.pack:
            frames = fonts = icons = 0
            with zipfile.ZipFile(path) as z:
                entries = set(z.namelist())
                for name in entries:
                    data = z.read(name) if not name.endswith('/') else b''
                    if name.endswith('.bmx'):
                        w, h = struct.unpack('<ii',data[:8])
                        self.assertTrue(decode(data[8:],w,h)[0],name)
                        icons += 1
                    if name.endswith('.u8f'):
                        self.assertTrue(font(data),name)
                        fonts += 1
                    if name.endswith('/meta.txt'):
                        fields = dict(line.split(': ',1) for line in data.decode().splitlines() if ': ' in line)
                        w,h = int(fields['Width']),int(fields['Height'])
                        for i in set(map(int,fields['Frames order'].split())):
                            frame = name.rsplit('/',1)[0] + f'/frame_{i}.bm'
                            self.assertTrue(decode(z.read(frame),w,h)[0],frame)
                            frames += 1
            print(f'Original pack {path.name}: {frames} frames, {icons} icons, {fonts} fonts validated')

    def test_no_live_reload_and_static_icon_guard(self):
        root = Path(__file__).resolve().parents[1]
        app = (root/'applications/main/momentum_app/momentum_app.c').read_text()
        self.assertNotIn('asset_packs_free()',app)
        self.assertIn('app->require_reboot || app->apply_pack',app)
        animation = (root/'components/gui/icon_animation.c').read_text()
        self.assertIn('instance->icon->frame_count <= 1 || instance->icon->frame_rate == 0',animation)


if __name__ == '__main__':
    unittest.main(argv=[__file__], verbosity=2)
