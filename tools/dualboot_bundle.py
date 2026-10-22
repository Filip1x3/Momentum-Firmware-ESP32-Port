#!/usr/bin/env python3
"""Validate and package segmented dual-boot binaries. Does not flash/push.

The bundle deliberately has no merged .bin: padding across 0x9000 would erase
the user's Momentum NVS/U2F identity. Never use a normal OTA package for this pair.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = [
    ("nvs", 1, 2, 0x9000, 0x6000),
    ("phy_init", 1, 1, 0xF000, 0x1000),
    ("ota_0", 0, 0x10, 0x10000, 0x5F0000),
    ("ota_1", 0, 0x11, 0x600000, 0x5F0000),
    ("otadata", 1, 0, 0xBF0000, 0x2000),
    ("coredump", 1, 3, 0xBF2000, 0x20000),
    ("bruce_nvs", 1, 2, 0xC12000, 0x10000),
    ("spiffs", 1, 0x82, 0xC22000, 0x3DE000),
]
WRAPPERS = (
    "nvs_flash_init", "nvs_flash_deinit", "nvs_flash_erase", "nvs_open",
    "nvs_flash_init_partition", "nvs_flash_deinit_partition",
    "nvs_flash_erase_partition", "nvs_open_from_partition", "esp_ota_begin",
    "esp_partition_write", "esp_partition_write_raw", "esp_partition_erase_range",
)

def require(condition, message):
    if not condition:
        raise ValueError(message)

def read_csv(path: Path):
    entries = []
    subtypes = {"nvs": 2, "phy": 1, "ota_0": 0x10, "ota_1": 0x11,
                "ota": 0, "coredump": 3, "spiffs": 0x82}
    for row in csv.reader(path.read_text(encoding="utf-8").splitlines()):
        if not row or row[0].lstrip().startswith("#"):
            continue
        name, kind, subtype, offset, size = [s.strip() for s in row[:5]]
        entries.append((name, {"app": 0, "data": 1}[kind], subtypes[subtype], int(offset, 0), int(size, 0)))
    require(entries == EXPECTED, f"Unsafe or mismatched dual-boot CSV: {path}")
    return entries

def read_table(path: Path):
    data = path.read_bytes()
    entries = []
    for pos in range(0, len(data), 32):
        record = data[pos:pos + 32]
        require(len(record) == 32, "Truncated partition table")
        magic = struct.unpack_from("<H", record)[0]
        if magic == 0xEBEB:
            require(record[16:] == hashlib.md5(data[:pos]).digest(), "Partition-table MD5 mismatch")
            require(entries == EXPECTED, f"Wrong partition table: {path}")
            return entries
        require(magic == 0x50AA, "Partition table has no valid MD5 record")
        _, kind, subtype, address, size, label, flags = struct.unpack("<HBBII16sI", record)
        require(flags == 0, "Unexpected partition flags")
        entries.append((label.split(b"\0", 1)[0].decode("ascii"), kind, subtype, address, size))
    raise ValueError("Partition table missing checksum")

def inspect_image(path: Path, project: str | None = None, max_size: int = 0x5F0000):
    data = path.read_bytes()
    require(24 <= len(data) <= max_size, f"Image missing/oversized: {path}")
    require(data[0] == 0xE9 and 1 <= data[1] <= 16, f"Not an ESP application: {path}")
    require(struct.unpack_from("<H", data, 12)[0] == 9, f"Not ESP32-S3: {path}")
    require(data[23] == 1, f"Image must carry a SHA-256 digest: {path}")
    checksum = 0xEF
    position = 24
    for _ in range(data[1]):
        require(position + 8 <= len(data), "Truncated segment header")
        _, size = struct.unpack_from("<II", data, position)
        position += 8
        require(position + size <= len(data), "Truncated image segment")
        for byte in data[position:position + size]:
            checksum ^= byte
        position += size
    checksum_pos = ((position + 16) // 16) * 16 - 1
    require(checksum_pos < len(data) and data[checksum_pos] == checksum, "Image checksum mismatch")
    digest_offset = checksum_pos + 1
    require(len(data) == digest_offset + 32, "Image is truncated or has unexpected trailing data")
    require(hashlib.sha256(data[:digest_offset]).digest() == data[digest_offset:], "Image SHA-256 mismatch")
    info = {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if project is not None:
        require(len(data) >= 288 and struct.unpack_from("<I", data, 32)[0] == 0xABCD5432,
                "Application descriptor missing")
        version = data[48:80].split(b"\0", 1)[0].decode("ascii")
        name = data[80:112].split(b"\0", 1)[0].decode("ascii")
        require(name == project and version.endswith("-db1"), f"Not the paired Dual Boot image: {path}")
        info.update(project=name, version=version)
    return info

def check_write_ranges(parts):
    protected = [(0x9000, 0x10000), (0xC12000, 0x1000000)]
    previous_end = 0
    for offset, data in sorted(parts, key=lambda item: item[0]):
        end = offset + len(data)
        require(offset >= previous_end, "Overlapping flash files")
        require(end <= 0x1000000, "Write exceeds 16 MiB flash")
        require(all(end <= start or offset >= stop for start, stop in protected),
                "Flash files would overwrite user NVS/PHY/Bruce settings")
        previous_end = end

def make_bundle(momentum: Path, bruce: Path, output: Path):
    read_csv(ROOT / "partitions_dualboot_16mb.csv")
    read_csv(bruce / "partitions_momentum_dualboot.csv")
    bruce_build = bruce / ".pio/build/momentum-dualboot"
    require(not (bruce_build / "firmware.factory.bin").exists(),
            "Unsafe SDK factory image was not quarantined; use only the segmented dual-boot build")
    read_table(momentum / "partition_table/partition-table.bin")
    read_table(bruce_build / "partitions.bin")
    momentum_info = inspect_image(momentum / "furi_esp32.bin", "furi_esp32")
    bruce_info = inspect_image(bruce_build / "firmware.bin", "bruce_mntm_db1")
    inspect_image(momentum / "bootloader/bootloader.bin", max_size=0x8000)
    boot_selector = (momentum / "ota_data_initial.bin").read_bytes()
    require(boot_selector == b"\xff" * 8192, "OTA selector must initially choose Momentum (ota_0)")
    sdkconfig = (momentum / "sdkconfig").read_text(encoding="utf-8")
    require('CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_dualboot_16mb.csv"' in sdkconfig,
            "Momentum was not built with the dual-boot partition table")
    map_text = (bruce_build / "firmware.map").read_text(encoding="utf-8", errors="replace")
    for name in WRAPPERS:
        require("__wrap_" + name in map_text, f"Missing NVS/flash isolation wrapper: {name}")
    from elftools.elf.elffile import ELFFile
    with (bruce_build / "firmware.elf").open("rb") as file:
        elf = ELFFile(file)
        live = {symbol.name for symbol in elf.get_section_by_name(".symtab").iter_symbols()
                if symbol["st_shndx"] != "SHN_UNDEF" and symbol["st_value"] != 0}
    for name in WRAPPERS:
        require("__wrap_" + name in live, f"Wrapper not present in the linked companion: {name}")
    sources = [
        (0, "bootloader.bin", momentum / "bootloader/bootloader.bin"),
        (0x8000, "partition-table.bin", momentum / "partition_table/partition-table.bin"),
        (0x10000, "momentum-dualboot.bin", momentum / "furi_esp32.bin"),
        (0x600000, "bruce-dualboot.bin", bruce_build / "firmware.bin"),
        (0xBF0000, "ota_data_initial.bin", momentum / "ota_data_initial.bin"),
    ]
    check_write_ranges([(offset, path.read_bytes()) for offset, _, path in sources])
    require(not output.exists(), f"Output already exists; choose a new folder: {output}")
    output.mkdir(parents=True)
    files = []
    for offset, name, source in sources:
        shutil.copy2(source, output / name)
        files.append({"offset": hex(offset), "file": name, "size": source.stat().st_size,
                      "sha256": hashlib.sha256(source.read_bytes()).hexdigest()})
    manifest = {"board": "LilyGo T-Embed CC1101", "chip": "esp32s3", "flash_size": "16MB",
                "status": "EXPERIMENTAL - hardware test required", "layout": "momentum-bruce-db1",
                "momentum": momentum_info, "bruce": bruce_info, "files": files}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    instructions = [
        "EXPERIMENTAL MOMENTUM / BRUCE DUAL BOOT",
        "Board: LilyGo T-Embed CC1101 (ESP32-S3, 16 MiB)", "",
        "Back up the SD card and important device data BEFORE installation.",
        "Never use erase-flash or a merged binary. These files preserve NVS at 0x9000.",
        "Use only the Dual Boot-aware installer or these exact serial offsets.",
        "Normal OTA, stock Bruce and normal/merged packages are not compatible.", "",
        "Flash these FIVE separate files in one esptool write-flash operation:",
        *[f"{item['offset']:>10}  {item['file']}" for item in files], "",
        "The boot selector intentionally starts Momentum. /bruce is created on SD.",
        "Enter: Settings > Dual Boot > Bruce (Dual Boot is below Clock & Alarm).",
        "Return: Bruce main menu > Momentum Port > Go back to Momentum Port.",
        "Bruce requires a writable SD; a failed SD mount returns to Momentum.",
        "Bruce's raw USB mass-storage mode is disabled: it cannot isolate an SD folder.",
        "Use Momentum's USB storage mode or a card reader to transfer SD files.",
    ]
    (output / "FLASH_OFFSETS.txt").write_text("\n".join(instructions) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--momentum-build", type=Path, required=True)
    parser.add_argument("--bruce-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    make_bundle(args.momentum_build.resolve(), args.bruce_dir.resolve(), args.output.resolve())
