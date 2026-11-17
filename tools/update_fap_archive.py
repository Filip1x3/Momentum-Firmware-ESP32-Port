"""Replace three verified crash-fix FAPs without rebuilding the SD package.

Unchanged ZIP local records are copied verbatim, including compressed bytes.
Names/order, timestamps, compression methods, attributes and archive comments
are retained. Only the three FAP records, their Manifest MD5/length records,
and files.txt SHA256/length records (including Manifest itself) may change.
The existing sdcard-package.json has no per-FAP hashes; its FAP/FAL
count is checked but the document is not rewritten.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "apps/Tools/caesar_cipher.fap": (
        "caesar_cipher.fap", 5980,
        "fbd2c2bc38987e43341e70fff1ea3cf88dd4ddbf6ccbc2f4d6a28583821944a4",
    ),
    "apps/Games/bomberduck.fap": (
        "bomberduck.fap", 41892,
        "cb7ec03dab234e763a0c408cbd4f353138913e86501295f40ff04c9ae31d8e94",
    ),
    "apps/SubGhz/proto_pirate.fap": (
        "proto_pirate.fap", 213392,
        "0b266ed882be7cc39e2137396e610ec9be3985e668b5ef85ef518697fa0f2031",
    ),
}
META_FIELDS = (
    "filename", "date_time", "compress_type", "comment", "extra",
    "create_system", "create_version", "extract_version", "reserved",
    "flag_bits", "volume", "internal_attr", "external_attr",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def manifest_update(original: bytes, archive: zipfile.ZipFile,
                    replacements: dict[str, bytes]) -> bytes:
    changed = set()
    lines = []
    for line in original.splitlines(keepends=True):
        body = line.rstrip(b"\r\n")
        fields = body.split(b":", 3)
        if len(fields) == 4 and fields[0] == b"F":
            name = fields[3].decode("utf-8")
            if name in replacements:
                require(name not in changed, f"Duplicate Manifest record: {name}")
                previous = archive.read(name)
                require(fields[1].decode() == hashlib.md5(previous).hexdigest() and
                        int(fields[2]) == len(previous), f"Stale original Manifest: {name}")
                payload = replacements[name]
                fields[1] = hashlib.md5(payload).hexdigest().encode("ascii")
                fields[2] = str(len(payload)).encode("ascii")
                line = b":".join(fields) + line[len(body):]
                changed.add(name)
        lines.append(line)
    require(changed == set(replacements), "Manifest does not cover exactly the three FAPs")
    return b"".join(lines)


def record_bounds(archive: zipfile.ZipFile) -> dict[str, tuple[int, int]]:
    infos = archive.infolist()
    offsets = [entry.header_offset for entry in infos] + [archive.start_dir]
    require(offsets[0] == 0, "Self-extracting/prefixed ZIPs are outside this updater's scope")
    require(all(left < right for left, right in zip(offsets, offsets[1:])),
            "ZIP local-record order differs from directory order")
    return {entry.filename: (offsets[index], offsets[index + 1])
            for index, entry in enumerate(infos)}


def checksums_update(original: bytes, archive: zipfile.ZipFile,
                     replacements: dict[str, bytes]) -> bytes:
    changed = set()
    lines = []
    for line in original.splitlines(keepends=True):
        body = line.rstrip(b"\r\n")
        fields = body.split(b" ", 2)
        if len(fields) == 3:
            name = fields[2].decode("utf-8")
            if name in replacements:
                require(name not in changed, f"Duplicate files.txt record: {name}")
                previous = archive.read(name)
                require(fields[0].decode() == digest(previous) and
                        int(fields[1]) == len(previous), f"Stale original files.txt: {name}")
                payload = replacements[name]
                fields[0] = digest(payload).encode("ascii")
                fields[1] = str(len(payload)).encode("ascii")
                line = b" ".join(fields) + line[len(body):]
                changed.add(name)
        lines.append(line)
    require(changed == set(replacements), "files.txt must cover the three FAPs and Manifest")
    return b"".join(lines)


def raw_record(archive: zipfile.ZipFile, bounds: tuple[int, int]) -> bytes:
    start, end = bounds
    archive.fp.seek(start)
    data = archive.fp.read(end - start)
    require(len(data) == end - start, "Truncated ZIP local record")
    return data


def write_archive(source: zipfile.ZipFile, destination: Path,
                  replacements: dict[str, bytes]) -> None:
    bounds = record_bounds(source)
    with zipfile.ZipFile(destination, "w") as result:
        result.comment = source.comment
        for info in source.infolist():
            saved = copy.copy(info)
            if info.filename in replacements:
                result.writestr(saved, replacements[info.filename])
            else:
                # ZipFile cannot preserve compressed bytes through writestr.
                # Copy the complete local record, then register its unchanged
                # metadata at the new offset for central-directory generation.
                saved.header_offset = result.fp.tell()
                result.fp.write(raw_record(source, bounds[info.filename]))
                result.filelist.append(saved)
                result.NameToInfo[saved.filename] = saved
                result.start_dir = result.fp.tell()
                result._didModify = True


def verify_archive(original: zipfile.ZipFile, changed_path: Path,
                   replacements: dict[str, bytes]) -> tuple[list[str], int]:
    old_bounds = record_bounds(original)
    with zipfile.ZipFile(changed_path) as result:
        require(result.testzip() is None, "New ZIP failed CRC verification")
        require(original.namelist() == result.namelist(), "ZIP entry names/order changed")
        require(original.comment == result.comment, "ZIP archive comment changed")
        new_bounds = record_bounds(result)
        changed = []
        for old_info, new_info in zip(original.infolist(), result.infolist()):
            name = old_info.filename
            for field in META_FIELDS:
                require(getattr(old_info, field) == getattr(new_info, field),
                        f"ZIP metadata changed: {name} / {field}")
            before, after = original.read(name), result.read(name)
            if name in replacements:
                require(after == replacements[name], f"Replacement mismatch: {name}")
                require(before != after, f"Requested entry was already current: {name}")
                changed.append(name)
            else:
                require(before == after, f"Unrelated contents changed: {name}")
                require(raw_record(original, old_bounds[name]) ==
                        raw_record(result, new_bounds[name]),
                        f"Unrelated compressed record changed: {name}")
        require(set(changed) == set(replacements), "Unexpected changed-entry set")
        return changed, len(result.infolist()) - len(changed)


def update(archive_path: Path, fap_dir: Path, backup_path: Path) -> dict:
    require(archive_path.is_file(), "Original SD archive does not exist")
    require(backup_path.resolve() != archive_path.resolve(), "Backup must be a separate file")
    replacement_data = {}
    for entry, (filename, size, sha256) in EXPECTED.items():
        data = (fap_dir / filename).read_bytes()
        require(len(data) == size and digest(data) == sha256,
                f"FAP is not the reviewed build: {filename}")
        replacement_data[entry] = data
    before_sha = digest(archive_path.read_bytes())
    backup_path.parent.mkdir(parents=True, exist_ok=True)
    # Never overwrite an older recovery copy, even on accidental reruns. Reuse
    # only an identical backup left by an earlier validation-only failure.
    if backup_path.exists():
        require(digest(backup_path.read_bytes()) == before_sha,
                "Existing backup differs from the current archive; refusing to overwrite it")
    else:
        with archive_path.open("rb") as source, backup_path.open("xb") as backup:
            shutil.copyfileobj(source, backup)
        shutil.copystat(archive_path, backup_path)
    require(digest(backup_path.read_bytes()) == before_sha, "Backup verification failed")

    temp_path = None
    try:
        with zipfile.ZipFile(backup_path) as original:
            require(original.testzip() is None, "Original ZIP failed CRC verification")
            require(len(original.namelist()) == len(set(original.namelist())),
                    "Duplicate names in original ZIP")
            require(set(EXPECTED).issubset(original.namelist()), "A target FAP is absent")
            package_bytes = original.read("sdcard-package.json")
            package = json.loads(package_bytes)
            # build_sdcard_momentum.py's historical fap_count includes plugins.
            fap_count = sum(name.lower().endswith((".fap", ".fal"))
                            for name in original.namelist())
            require(package.get("fap_count") == fap_count, "Stale package FAP count")
            require(not any(name.encode() in package_bytes or filename.encode() in package_bytes
                            for name, (filename, _, _) in EXPECTED.items()),
                    "Per-FAP package metadata needs an explicit schema update")
            replacement_data["Manifest"] = manifest_update(
                original.read("Manifest"), original, replacement_data)
            require(not any(line.endswith(b":files.txt")
                            for line in original.read("Manifest").splitlines()),
                    "Unexpected circular manifest/checksum layout")
            replacement_data["files.txt"] = checksums_update(
                original.read("files.txt"), original, replacement_data)
            with tempfile.NamedTemporaryFile(prefix=archive_path.name + ".",
                                             suffix=".tmp", dir=archive_path.parent,
                                             delete=False) as temporary:
                temp_path = Path(temporary.name)
            write_archive(original, temp_path, replacement_data)
            changed, unchanged = verify_archive(original, temp_path, replacement_data)
        require(digest(archive_path.read_bytes()) == before_sha,
                "Original archive changed during preparation; refusing to overwrite it")
        os.replace(temp_path, archive_path)
        temp_path = None
        return {
            "archive": str(archive_path.resolve()), "backup": str(backup_path.resolve()),
            "old_sha256": before_sha, "new_sha256": digest(archive_path.read_bytes()),
            "bytes": archive_path.stat().st_size, "changed_entries": changed,
            "unchanged_entries_byte_for_byte_including_compression": unchanged,
            "sdcard_package_json": "unchanged: no per-FAP hashes, combined FAP/FAL count remains " + str(fap_count),
            "zip_crc": "PASS", "metadata_order_names_compression": "PASS",
        }
    finally:
        if temp_path is not None:
            # Only remove the uniquely created temporary file, never an archive
            # or backup supplied by the caller.
            require(temp_path.parent.resolve() == archive_path.parent.resolve() and
                    temp_path.name.startswith(archive_path.name + ".") and
                    temp_path.suffix == ".tmp", "Unsafe temporary cleanup path")
            temp_path.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=ROOT / "sdcard.zip")
    parser.add_argument("--fap-dir", type=Path,
                        default=ROOT.parent / ".diagnostics/security-build-tembed/fap")
    parser.add_argument("--backup", type=Path,
                        default=ROOT.parent / ".diagnostics/sdcard-before-fixes-20261023.zip")
    args = parser.parse_args()
    print(json.dumps(update(args.archive, args.fap_dir, args.backup), indent=2))


if __name__ == "__main__":
    main()
