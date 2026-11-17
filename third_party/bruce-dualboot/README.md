# Bruce Dual Boot corresponding source

## Current stable companion: 1.17

[bruce-dualboot-source-1.17.zip](bruce-dualboot-source-1.17.zip) contains the
complete modified stable companion source, GPL-3.0 license, dependency/build
configuration, pinned submodule sources and local regression tests. Upstream base
is official release `1.17`, published on 2026-11-12, commit
`2b6ba4d490bfcf94ce2574c6154537dbd5838c3d`. UI version is `1.17`;
the isolated ESP-IDF descriptor remains `bruce_mntm_db1` / `1.17-db1`.

Extract the archive into a new dedicated `Bruce-Momentum-DualBoot-Stable-1.17`
directory. Follow `README-DUALBOOT-RELEASE.md` inside it. The port's build helper
validates the upstream pin, source hashes and stable version identifiers.
Only source inputs are included; no `.git`, `.pio`, device data or private keys.

Archive SHA-256: `9477634efaa13ff6a1348ae548f7f6e62be7918f81276e00b0b9fce83edb8e39`.
This is the corresponding source for the Bruce companion in Momentum ESP32 Port
v0.2.1, not an installer. Firmware BINs are available in the
[v0.2.1 release](https://github.com/Filip1x3/Momentum-Firmware-ESP32-Port/releases/tag/v0.2.1).

## Previous stable companion: 1.16.1

[bruce-dualboot-source-1.16.1.zip](bruce-dualboot-source-1.16.1.zip) retains the
previous companion based on commit `ba519c936c87b89c9c667c15b932d7faca5360d5`.
Archive SHA-256: `f2fa8ce198d9bc6bc938a7bb07db99c25d3311424f8f50b36eaf33a857187bbc`.

## Historical v0.2.0 companion: 1.16

[bruce-dualboot-source-v0.2.0.zip](bruce-dualboot-source-v0.2.0.zip) contains the
complete modified source for the Bruce 1.16-db1 companion distributed with
Momentum ESP32 Port v0.2.0, including build configuration, dependency definitions,
upstream copyright notices and the GPL-3.0 license.

Upstream: [BruceDevices/firmware](https://github.com/BruceDevices/firmware),
base commit `59e83bfbd8a63a6b67ea23498e15c710a1ed9657`.
Extract it and follow `README-DUALBOOT-RELEASE.md`. `RELEASE-SOURCE.json` records
the base commit and hashes all source files; use the helper from the matching
v0.2.0 port tag to reproduce this historical companion.

Archive SHA-256: `5d28d6f6abf0d0d1a9bd96adcc72e86d294fc0ef9d1d0201ccfe16e71f61f858`.

This archive was moved unchanged from the release assets so that the download
list contains only firmware BINs and sdcard.zip. This is source, not an installer.
