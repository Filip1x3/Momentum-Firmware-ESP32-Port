#!/usr/bin/env python3
"""Export corresponding companion source, never .git, .pio or device data."""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

BASE = "59e83bfbd8a63a6b67ea23498e15c710a1ed9657"
README = """# Bruce companion source for Momentum ESP32 Port v0.2.0

This is the complete modified source of the Bruce 1.16-db1 companion, based on
BruceDevices/firmware commit 59e83bfbd8a63a6b67ea23498e15c710a1ed9657.
Original copyright and license notices are retained. See LICENSE (GPL-3.0).
RELEASE-SOURCE.json identifies the upstream base and hashes every exported file.

The integration changes SD paths to /bruce, isolates Bruce's NVS/flash writes,
adds the Momentum Port return menu and logo, and disables standalone upload,
unsafe merged installers, normal OTA and raw USB storage. No attack modules
were changed by this integration. This is not a sandbox for arbitrary code.

## Build

Install PlatformIO Core with the dependencies defined in platformio.ini and
export ESP-IDF 5.4.1 for the matching Momentum build. Dependency downloads need
internet access. Preserve this source directory and its release manifest.
Download the Momentum ESP32 Port v0.2.0 source from:
https://github.com/Filip1x3/Momentum-Firmware-ESP32-Port/tree/v0.2.0

From the Momentum source directory in an exported ESP-IDF shell:

    python tools/build_dualboot.py --bruce-dir /absolute/Bruce-Momentum-DualBoot

The helper accepts either this verified source archive or the reviewed Git
checkout. It builds the momentum-dualboot PlatformIO environment and the
MOMENTUM_DUALBOOT=ON ESP-IDF profile, validates both partition tables and images,
and exports a segmented installer. Use --output for a new output directory.

Never run PlatformIO upload/erase or flash a stock Bruce/merged image. Install
the complete matching five-file package with the Dual Boot-aware web flasher,
or use the documented exact serial offsets. Back up SD and important data.

Upstream: https://github.com/BruceDevices/firmware
Port: https://github.com/Filip1x3/Momentum-Firmware-ESP32-Port
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bruce-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source, output = args.bruce_dir.resolve(), args.output.resolve()
    assert not output.exists(), "Choose a new output archive; do not overwrite releases"
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    assert commit == BASE, "Unexpected Bruce upstream base"
    raw = subprocess.check_output(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=source)
    names = sorted(set(item.decode("utf-8") for item in raw.split(b"\0") if item))
    files = {}
    for name in names:
        parts = PurePosixPath(name).parts
        assert parts and not PurePosixPath(name).is_absolute() and ".." not in parts, name
        assert not set(parts) & {".git", ".pio", ".diagnostics", "node_modules", "__pycache__"}, name
        path = source / name
        assert path.is_file() and not path.is_symlink(), name
        assert path.name.lower() not in {".env", "nvs.bin", "flash_dump.bin", "key.u2f", "cert_key.u2f"}, name
        assert path.suffix.lower() not in {".key", ".pfx", ".p12", ".keystore"}, name
        files[name] = path.read_bytes()
    assert "LICENSE" in files and "src/core/dualboot_nvs.c" in files and "platformio.ini" in files
    files["README-DUALBOOT-RELEASE.md"] = README.encode("utf-8")
    manifest = {"schema": 1, "upstream": "https://github.com/BruceDevices/firmware", "base_commit": BASE,
                "companion_version": "1.16-db1", "release": "v0.2.0", "files": {
                    name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in files.items():
            archive.writestr("Bruce-Momentum-DualBoot/" + name, data)
        archive.writestr("Bruce-Momentum-DualBoot/RELEASE-SOURCE.json", json.dumps(manifest, indent=2) + "\n")
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
    print(f"Exported {len(files)} corresponding source files, {output.stat().st_size} bytes; GPL license included")


if __name__ == "__main__":
    main()
