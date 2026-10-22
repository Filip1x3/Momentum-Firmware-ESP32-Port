#!/usr/bin/env python3
"""Build both local companions and create a validated, non-destructive bundle.

Requires an exported ESP-IDF 5.4.1 environment and the modified Bruce 1.16 tree.
No serial connection, flashing, Git push or website deployment is performed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path
from dualboot_bundle import ROOT, make_bundle, read_csv, require

BRUCE_COMMIT = "59e83bfbd8a63a6b67ea23498e15c710a1ed9657"

def check_bruce_source(bruce):
    if (bruce / ".git").exists():
        commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=bruce, text=True).strip()
        require(commit == BRUCE_COMMIT, "Bruce source must be the reviewed stable 1.16 commit")
        return
    metadata = json.loads((bruce / "RELEASE-SOURCE.json").read_text(encoding="utf-8"))
    require(metadata.get("schema") == 1 and metadata.get("base_commit") == BRUCE_COMMIT,
            "Use the matching corresponding-source archive from the firmware release")
    files = metadata.get("files", {})
    require("src/core/dualboot_nvs.c" in files and "platformio.ini" in files and "LICENSE" in files,
            "Incomplete companion source manifest")
    for name, expected in files.items():
        path = (bruce / name).resolve()
        require(path.is_relative_to(bruce) and path.is_file(), f"Missing/unsafe source path: {name}")
        require(hashlib.sha256(path.read_bytes()).hexdigest() == expected, f"Source snapshot changed: {name}")

def run(arguments, directory):
    print("Running:", subprocess.list2cmdline([str(arg) for arg in arguments]), flush=True)
    environment = os.environ.copy()
    if os.name == "nt":
        # A Bruce dependency has >260-character example paths. Keep this local
        # to child processes; do not change the user's global Git configuration.
        index = int(environment.get("GIT_CONFIG_COUNT", "0"))
        environment[f"GIT_CONFIG_KEY_{index}"] = "core.longpaths"
        environment[f"GIT_CONFIG_VALUE_{index}"] = "true"
        environment["GIT_CONFIG_COUNT"] = str(index + 1)
    subprocess.run([str(arg) for arg in arguments], cwd=directory, check=True, env=environment)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bruce-dir", type=Path, default=ROOT.parent / "Bruce-Momentum-DualBoot")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build_dualboot")
    parser.add_argument("--output", type=Path, default=ROOT / "build_dualboot_bundle")
    parser.add_argument("--pio", default="pio")
    parser.add_argument("--idf-python", default=sys.executable)
    args = parser.parse_args()
    bruce, build, output = args.bruce_dir.resolve(), args.build_dir.resolve(), args.output.resolve()
    require(not output.exists(), "Choose a new, empty output path; existing bundles are never overwritten")
    idf = Path(os.environ.get("IDF_PATH", "")) / "tools/idf.py"
    require(idf.is_file(), "Export ESP-IDF 5.4.1 first (IDF_PATH must be set)")
    check_bruce_source(bruce)
    read_csv(ROOT / "partitions_dualboot_16mb.csv")
    read_csv(bruce / "partitions_momentum_dualboot.csv")
    require((bruce / "src/core/dualboot_nvs.c").is_file(), "Use the modified companion, not stock Bruce")
    run([args.pio, "run", "-e", "momentum-dualboot"], bruce)
    run([args.idf_python, idf, "-B", build, "-D", f"SDKCONFIG={build / 'sdkconfig'}",
         "-D", "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32s3;sdkconfig.dualboot.defaults",
         "-D", "IDF_TARGET=esp32s3", "-D", "FLIPPER_BOARD=lilygo_t_embed_cc1101",
         "-D", "MOMENTUM_DUALBOOT=ON", "build"], ROOT)
    make_bundle(build, bruce, output)

if __name__ == "__main__": main()
