"""Package matching stable companion source, without build caches or credentials.

Does not build, flash, publish, or overwrite an existing archive.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import subprocess
import time
import zipfile

from build_dualboot import BRUCE_COMMIT, BRUCE_RELEASE, check_bruce_source


def package(source: Path, output: Path):
    source, output = source.resolve(), output.resolve()
    if output.exists() or output.is_relative_to(source):
        raise ValueError("Choose a new archive path outside the source tree")
    check_bruce_source(source)
    if not (source / ".git").is_dir():
        raise ValueError("Packaging requires the reviewed Git checkout")
    names = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=source)
    modes = {}
    # Stable 1.17 vendors display and NFC libraries as pinned submodules.
    # Export their tracked files as well so a source ZIP can build without .git.
    for entry in subprocess.check_output(
            ["git", "ls-files", "--stage", "--recurse-submodules", "-z"], cwd=source).split(b"\0"):
        if entry:
            header, name = entry.split(b"\t", 1)
            modes[name.decode("utf-8")] = int(header.split()[0], 8)
    files = {}
    source_names = set(n.decode("utf-8") for n in names.split(b"\0") if n) | set(modes)
    for name in sorted(source_names):
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts or ":" in name or "\\" in name:
            raise ValueError("Unsafe source name: " + name)
        if name == "RELEASE-SOURCE.json":
            continue
        if any(part in {".git", ".pio", "node_modules", "__pycache__", ".diagnostics"} for part in relative.parts):
            continue
        if (source / name).is_dir() and name not in modes:
            # A root gitlink is replaced by its recursively enumerated files.
            if not any(child.startswith(name + "/") for child in modes):
                raise ValueError("Uninitialized source submodule: " + name)
            continue
        if relative.name.lower() in {".env", "nvs.bin", "flash_dump.bin", "key.u2f", "cert_key.u2f"} or \
                relative.suffix.lower() in {".key", ".pfx", ".p12", ".keystore"}:
            raise ValueError("Private data must not be exported: " + name)
        path = (source / name).resolve()
        if not path.is_relative_to(source) or not path.is_file() or (source / name).is_symlink():
            raise ValueError("Missing or non-regular source file: " + name)
        files[name] = path.read_bytes()
    for required in ("LICENSE", "README-DUALBOOT-RELEASE.md", "platformio.ini", "src/core/dualboot_nvs.c"):
        if required not in files:
            raise ValueError("Missing corresponding source input: " + required)
    metadata = {"schema": 1, "upstream": "https://github.com/BruceDevices/firmware",
                "upstream_release": BRUCE_RELEASE, "companion_version": BRUCE_RELEASE + "-db1",
                "release": "unreleased", "base_commit": BRUCE_COMMIT,
                "files": {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}
    files["RELEASE-SOURCE.json"] = (json.dumps(metadata, indent=2) + "\n").encode()
    timestamp = int(subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=source))
    date = time.gmtime(timestamp)[:6]
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = modes.get(name, 0o100644) << 16
            archive.writestr(info, data)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise ValueError("Source archive CRC check failed")
        for name, digest in metadata["files"].items():
            if hashlib.sha256(archive.read(name)).hexdigest() != digest:
                raise ValueError("Source archive mismatch: " + name)
    print(json.dumps({"archive": str(output), "base_commit": BRUCE_COMMIT,
                      "source_files": len(metadata["files"]), "bytes": output.stat().st_size,
                      "sha256": hashlib.sha256(output.read_bytes()).hexdigest()}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", "--bruce-dir", dest="source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    package(args.source, args.output)
