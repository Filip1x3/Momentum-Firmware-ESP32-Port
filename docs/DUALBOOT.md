# Momentum / Bruce Dual Boot (experimental)

This is a separate build profile for **LilyGo T-Embed CC1101, ESP32-S3, 16 MiB flash / 8 MiB PSRAM**.
Available as the separate v0.2.0 Dual Boot installer. The updated web flasher
understands both layouts; old normal-only updaters do not. The normal firmware
profile retains its standard update layout and has no Dual Boot menu.
Switching in both directions and normal app entry have been checked on a
development device. The broader acceptance checklist below is not fully completed.

## Two separate firmware variants

Keep the normal and Dual Boot installers separate; do not mix their files.

- **Without Dual Boot:** the default build (`MOMENTUM_DUALBOOT=OFF`). Only
  Momentum is included, with the normal update layout and no Dual Boot menu.
- **With Dual Boot:** the opt-in profile (`MOMENTUM_DUALBOOT=ON`). Includes
  the matching isolated Bruce companion and the Dual Boot menu. Requires the
  five-file installer or the updated web flasher with **Dual Boot On**.

Both variants use the same board target, `lilygo_t_embed_cc1101`. Build them in
different directories so one cannot silently replace the other. Changing from
Dual Boot back to the normal variant requires its complete segmented installer
(including its partition table and boot selector), not only the app binary.
This changes the layout; make a private backup before either migration.

## User interface

- Momentum: **Settings → Dual Boot → Bruce**. Dual Boot is directly below Clock & Alarm, before About.
- Confirm Start. A writable SD card and a valid companion image are required.
- Bruce: **Momentum Port → Go back to Momentum Port**.
- The return item cannot be hidden through Bruce's Hide/Show Apps setting.
- The selected firmware remains selected after a restart/power cycle. If Bruce
  cannot mount/prepare the SD card at startup, it selects Momentum instead.
  If Momentum is also missing/corrupt, Bruce stops with an error; it does not
  loop between broken images or continue using the SD root.

## Storage and update safety

The existing NVS region at `0x9000` is retained, including Momentum's U2F keys,
PIN and Bluetooth bonds. Bruce's Arduino/IDF NVS calls are redirected (including
calls from static libraries) to `bruce_nvs` at `0xC12000`. Standard writes/erases to
the Momentum NVS or either application partition are rejected in the Bruce build.

Bruce sees `/` through the Arduino FS API as the physical SD directory `/bruce`.
File iteration returns logical paths, and `..`, backslashes and drive prefixes
are rejected. Its normal subfolders are created there without formatting the SD.
Configuration is also stored in a separate LittleFS partition. Existing Momentum
SD assets are not moved or rewritten. This is file separation, **not a security
sandbox for arbitrary native code**; both firmwares run with full MCU privileges.
Momentum's PIN does not lock Bruce; each system has its own settings and security model.

Raw USB mass storage in Bruce is disabled because exposing raw sectors cannot be
limited to `/bruce`. Use a card reader or Momentum's USB storage mode instead.
Normal in-device OTA is disabled: the other application slot is an operating
system, not an update target. `esp_ota_set_boot_partition` validates the complete
target image before switching. The descriptors distinguish the isolated Bruce
companion (`bruce_mntm_db1`) and Momentum (`furi_esp32`, version suffix `-db1`).
Checksums identify corruption, not a publisher signature or verified boot.

The boot-switch worker is initialized before services and apps start. Its 8 KiB
stack, task control block, request and synchronization buffers are reserved in
internal DRAM. Switching does not create a new dynamic task after asset packs,
Wi-Fi and other apps have fragmented/consumed the internal heap. The worker stays
blocked between requests, serializes callers and keeps its synchronization objects
alive across failures; it is not deleted and immediately recreated. These reserved
resources are excluded from the normal (non-Dual-Boot) build.

Never flash stock Bruce, a normal OTA update, a normal-only website package or a merged
image into this layout. A merged image starting at zero can overwrite the NVS
gap and permanently destroy the U2F identity. Exported bundles contain five
separate files and validated offsets; neither NVS nor LittleFS is included.

## Build locally

Sources used: [Bruce 1.16](https://github.com/BruceDevices/firmware/tree/1.16),
commit `59e83bfbd8a63a6b67ea23498e15c710a1ed9657` with the port modifications
in the [matching source archive](../third_party/bruce-dualboot/bruce-dualboot-source-v0.2.0.zip)
stored in this repository, outside the release's BIN/SD attachments.
The archive contains full source, dependency/build configuration, a file-hash
manifest and the upstream GPL-3.0 license. Retain all upstream license notices.
No attack modules are modified by the dual-boot integration.

In an exported ESP-IDF 5.4.1 shell with PlatformIO/pioarduino available:

```text
python tools/build_dualboot.py --bruce-dir ../Bruce-Momentum-DualBoot
```

This compiles the two app-only images, checks both partition tables, image
headers/checksums/SHA-256, companion descriptors and flash ranges, and creates
`build_dualboot_bundle`. It does not upload anything. Use a new `--output` folder
for another bundle. Do not use `idf.py flash` alone: that installs only Momentum,
not the Bruce companion. Standalone PlatformIO upload/erase targets are blocked.
The SDK's automatically merged `firmware.factory.bin` is renamed to `.bin.disabled`
by the build post-hook and is never included in the validated bundle. Do not rename
it back or flash it; its layout is not an installer for this firmware pair.

## Verification

Host tests:

`tools/tests/dualboot_worker/worker_test.c` executes the real boot-service source
against a fake scheduler/partition/SD layer. It covers unavailable/failed startup,
idempotent static initialization, layout/image/SD rejection, 1,000 repeated failed
requests, a concurrent-call busy guard and selection followed by restart. Dynamic
task allocation is forced to fail and must never be called by the boot service.
The test simulates scheduling and flash validation; it is not a physical switch.
On the Windows development machine, run `tools/tests/dualboot_worker/build.cmd`.
The built firmware ELF should place `switch_stack` (8192 bytes), task/control
buffers and request in internal DRAM, not in the 0x3c... PSRAM address range.

`tools/tests/dualboot_storage/storage_test.c`, linked with the actual
`components/fw_ota/fw_dualboot_storage.c`, covers 15 SD preflight scenarios.
Use its `stubs` include directory plus `applications/services/storage` and
`components/fw_ota`. The stubs use the real filesystem enum/type definitions.
In particular, the volume root must never be passed to `storage_common_stat`:
FatFs `f_stat` returns `FR_INVALID_NAME` for that root, even with a healthy card.
The helper checks `/ext/bruce` and exclusive create/write/sync/close/remove
instead; missing/read-only/full cards must still be rejected.

```text
python tools/test_dualboot_bundle.py
```

In the Bruce tree, compile/run `tests/dualboot/nvs_guard_test.c` with the include
directory `tests/dualboot/stubs` and compile/run `tests/dualboot/path_test.cpp`.
They exercise the actual wrapper and path-validation source, not copied models.
The stubs cover routing and allowed writes, not the real flash driver.
`python tests/dualboot/test_build_guard.py` in the Bruce tree also tests that
standalone upload/erase requests are refused without contacting a device.

Physical-device acceptance checklist (not all scenarios completed):

1. Back up the **whole flash and SD card locally** before installing the five
   segmented files. Full-flash backups contain private U2F keys/credentials;
   do not publish, commit, upload or share them.
2. Confirm Momentum boots first; existing PIN, U2F registration and SD assets work.
3. Remove/unmount SD: starting Bruce must show an error and leave Momentum selected.
4. Insert SD: enter Bruce; confirm its folder/config creation only under `/bruce`.
5. Change a harmless display setting, return using Momentum Port, then re-enter
   Bruce and confirm that setting persists. Repeat switching and power cycling.
6. Boot with Bruce selected and SD removed: it must return to Momentum.
7. Test file create/read/rename/delete and nested directory iteration in Bruce;
   confirm Momentum's files and U2F authentication are unchanged afterwards.
8. Confirm both firmware updaters cannot write the other app slot. Test USB,
   encoder/back input, display, SD remount, radio initialization and power-off
   on both battery and USB, without transmitting unsolicited radio traffic.

Do not claim full compatibility until this test is completed. Some upstream
Bruce functions require external hardware or additional SD resources; creating
directories does not install those resources or add hardware to the T-Embed.
Upstream dependency warnings (audio codec bounds and TFT/NimBLE ODR diagnostics)
remain in the Bruce build log. They are not a completed security audit of Bruce
or a guarantee that every upstream application works on this board.
