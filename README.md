# Momentum Firmware ESP32 Port

Momentum-style firmware for **LilyGo T-Embed CC1101**, maintained by
[Filip1x3](https://github.com/Filip1x3).

Based on [Sor3nt's Flipper Zero ESP32 Port](https://github.com/Sor3nt/Flipper-Zero-ESP32-Port),
with code, interface components and assets adapted from
[Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware).
This is an independent project, not an official Momentum or Flipper Devices release.

[Web Flasher](https://momentum-esp32-port.pages.dev/)
 · [Download firmware and SD files](https://github.com/Filip1x3/Momentum-Firmware-ESP32-Port/releases)
 · [Security notes](SECURITY.md) · [Security review](SECURITY-REVIEW.md)

## On the device

<p align="center">
  <img src="docs/images/tembed-boot.PNG" width="280" alt="Momentum boot screen on T-Embed CC1101">
  <img src="docs/images/tembed-menu.PNG" width="280" alt="Momentum main menu on T-Embed CC1101">
  <img src="docs/images/tembed-control-center.PNG" width="280" alt="Momentum Control Center on T-Embed CC1101">
</p>

Photos show the port running on a real T-Embed CC1101. Colors and graphics depend
on the selected screen theme and asset pack.

## Supported board

| Board | MCU | Display | Controls | Storage |
|---|---|---|---|---|
| **LilyGo T-Embed CC1101** | ESP32-S3 | 320 × 170 color LCD | Rotary encoder, encoder button and Back button | microSD |

This is the only supported board for this fork and its release binaries.
Inherited source files for other boards are not a support commitment.
Peripheral support depends on the hardware actually fitted or connected;
this port does not give the ESP32 every peripheral of a Flipper Zero.

## What changed from the base ESP32 port

### v0.2.0 update

- Two separate installers: **Momentum only** and **Momentum + Bruce Dual Boot**.
  Select the variant in the web flasher; do not mix their BIN files.
- Dual Boot appears below Clock & Alarm in Settings. Starting Bruce requires a
  writable SD card; its files are separated under `/bruce`. Return through
  **Momentum Port → Go back to Momentum Port** in Bruce.
- Fixes for spontaneous resets and app-entry crashes: corrected task-local
  storage ownership, Bluetooth memory lifecycle and asset/animation validation.
- Faster Sub-GHz startup through buffered file reads; NFC Read/Detect no longer
  loads card parsers before beginning detection. Card parsing loads them on demand.
- USB Storage now opens its full-screen view from **Settings → System → Others**.
- Public GitHub release selection in the web flasher, with matching offline
  fallback packages. Normal and Dual Boot manifests/layouts are checked separately.

See [Dual Boot](docs/DUALBOOT.md), [startup and USB storage](docs/STARTUP_AND_USB_STORAGE.md),
[Bluetooth memory](docs/BT_MEMORY_STABILITY.md) and [asset-pack stability](docs/ASSET_PACK_STABILITY.md).
Dual Boot is experimental; verified switching on a development device is not
a guarantee that every Bruce application or third-party asset pack works.

### Momentum interface

- Dedicated **Momentum** menu with Interface, Protocols and Misc settings.
  Interface customization lives here instead of the old standalone Settings entry.
- Momentum boot logo and loading screen, optional intro and screen-theme support.
- Nine main-menu styles: **List, Wii, DSi, PS4, Vertical, C64, Compact, MNTM and CoverFlow**.
- Momentum-style Control Center adapted to the rotary encoder, with brightness,
  volume, lock and system shortcuts. Short-click brightness to adjust it;
  hold for about 2.5 seconds to open screen settings.
- Status-bar customization, battery styles and a muted indicator when sound is off.
- Momentum asset-pack loading for desktop animations, icons and Passport graphics.
  Packs downloaded from Momentum can be used without manually editing their files.
  Pack changes are applied with a restart. External bitmaps, animation metadata and
  fonts are checked before use; oversized replacement icons fit their original UI
  boxes in memory, without modifying the pack on the SD card.
- Dolphin level, XP and mood settings, Passport customization, screen colors,
  dark mode and device-name/shell-color options.
- Menu text scrolling, browser display options and optional Popup Overlay
  (disabled by default).

### Desktop, lock screen and settings

- Keypad lock, PIN setup, automatic locking, Auto Lock Pin and lock-on-boot options.
- Keypad unlock uses three Back presses. The locked screen stays dimmed rather
  than repeatedly switching off and on.
- Lock-screen clock/date/status options and animation controls.
- Locked USB/BLE RPC permissions and optional unlock from a trusted paired phone.
  Allowing remote unlock can bypass the device PIN; enable it only deliberately.
- Optional **Format on Bad PINs**, with an explicit warning/confirmation.
  Repeated incorrect PINs can erase SD-card contents when this is enabled.
- Reorganized Settings, including **LCD and Notifications**, Desktop, Clock/Alarm,
  Storage, Power, System, Others, qFlipper Settings and a port-specific About page.
- LED color, brightness, effects, speed and timeout settings for T-Embed.

### USB, Bluetooth and SD integration

- **U2F** in the main menu, using USB HID and a per-device software identity in NVS.
  Startup warnings explain that this is an experimental authenticator.
- **Bad KB**, the Ducky-script runner adapted from the base port, with USB/BLE
  transport selection. This is a script runner, not an on-screen typing keyboard.
- qFlipper-compatible USB RPC bridge with a saved enable-on-boot preference.
- Phone remote control and trusted-phone unlock, subject to pairing and locked-RPC settings.
- USB Storage under **Settings → System → Others**, with an immediate full-screen storage view.
  USB Storage, U2F, Bad KB and qFlipper may need exclusive use of the USB interface.
- WiFi enable/disable and Mesh Clients moved into WiFi; Bluetooth enable/disable
  is available in Bluetooth. Toggle labels reflect the current state.
- Streaming grouped under Sub-GHz instead of occupying the main menu.
- Prepared **sdcard.zip** with port-compatible applications, application assets,
  databases and asset packs. FAP loader/API compatibility fixes are included in firmware.

### Security hardening

- Web-Filesystem login on every route: username **admin**, with a random session
  password shown on the device. Restart the server to change the password.
- SD-root/path validation, cross-origin request checks, locked-device checks and
  bounded uploads for Web-Filesystem.
- Certificate/hostname verification for network updater connections and safer
  updater cancellation handling.
- Network firmware/SD updates remain disabled until a supported release feed is
  configured. Publishing a GitHub Release does not enable in-device downloads.
  No private GitHub token is embedded in the firmware.

## Applications and hardware features

The base port's application framework and hardware integrations remain available:

- **Sub-GHz:** CC1101 receive/decode, RAW capture, saved signals, frequency/spectrum
  tools and remote profiles. Protocol support is not identical to Flipper hardware.
- **WiFi:** scanning, saved connections, capture/diagnostic tools, Web-Filesystem,
  network utilities and ESP-NOW buddy integration.
- **Bluetooth:** device discovery and the base port's BLE/HID tools.
- **Infrared:** receive/learn, saved remotes and SD-based universal remote databases.
- **NFC:** PN532-based functions where the required hardware is available;
  supported operations depend on the card technology and port implementation.
- **Files:** SD browsing, favorites, copy/paste, rename, delete and folders.
- **Apps and games:** ESP32-compatible FAP applications, including the existing
  Doom port. Doom requires its WAD data on the SD card.
- **JavaScript:** the port's existing mJS runtime and available module bindings;
  Flipper-specific hardware bindings are not all implemented.

Use radio, network and HID tools only on devices/networks you own or have
permission to test. Included features are not a guarantee that every protocol,
card, peripheral or third-party app works.

## Install

Use the **[Web Flasher](https://momentum-esp32-port.pages.dev/)** to install
firmware over USB from a desktop browser with Web Serial, such as Chrome or Edge.
Select LilyGo T-Embed CC1101, choose the firmware and installation mode, connect
the device and confirm installation. Close qFlipper and other serial clients first.
Select **Dual Boot Off** for Momentum only, or **On** for Momentum + Bruce.
The hosted installer reads complete public GitHub releases automatically and
labels its included v0.2.0 fallback explicitly when GitHub is unavailable.
Download and extract the matching SD-card package after flashing.

Back up your SD card and any important device data before flashing.
Download the matching installer ZIP and `sdcard.zip` from
[Releases](https://github.com/Filip1x3/Momentum-Firmware-ESP32-Port/releases).
Use binaries from this fork, not upstream Flipper/Momentum firmware or the
upstream Sor3nt web flasher.

### Full serial flash: standard variant only

Close qFlipper and other programs using the USB port. Put T-Embed into its
firmware-download mode, then flash these files at their exact hexadecimal offsets:

| File | Offset |
|---|---|
| `bootloader.bin` | `0x0` |
| `partition-table.bin` | `0x8000` |
| `furi_esp32.bin` | `0x10000` |
| `ota_data_initial.bin` | `0xBF0000` |

Flash settings: **ESP32-S3 / 16 MB / DIO / 80 MHz**.
These offsets belong to this release's dual-OTA partition layout.
`ota_data_initial.bin` does **not** belong at `0x10000`.
For the separate five-file Dual Boot installer, use the filenames/offsets in
the release's `FLASHING.md` and [Dual Boot notes](docs/DUALBOOT.md), not this table.

Example using esptool 4.x from the ESP-IDF 5.4.1 environment
(replace `COM8` with your device's port):

```sh
python -m esptool --chip esp32s3 --port COM8 --baud 460800 write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 furi_esp32.bin 0xBF0000 ota_data_initial.bin
```

This is a **full serial installation**, not an instruction for an in-app OTA update.
Do not erase the entire flash unless you intend to lose settings, pairing and U2F
credentials. Do not overwrite OTA metadata during an ordinary in-app update.

### SD card

Extract the **contents** of `sdcard.zip` to the root of a FAT32 microSD card.
Do not put the entire ZIP or an extra enclosing `sdcard` directory on the card.
Keep both `apps` and `apps_assets`, as well as the other supplied folders.
Merge carefully with an existing card: replacing files can overwrite custom data.

| Folder / location | Purpose |
|---|---|
| `apps/` | Port-compatible FAP applications |
| `apps_assets/` | Resources required by applications |
| `apps_data/` | Application data and plugin resources |
| `asset_packs/` | Momentum desktop/icon packs |
| `dolphin/` | Desktop animation resources |
| `badusb/` | Bad KB scripts and keyboard layouts |
| `infrared/`, `nfc/`, `subghz/` | Protocol databases and saved data |
| Device NVS, not the SD card | U2F identity/counter and device settings |

Flipper Zero `.fap` binaries target a different processor and cannot simply be
copied across. Use ESP32 builds compatible with this port's API. Included apps
can still have individual limitations; compatibility is not universal.

## Build from source

Use **ESP-IDF 5.4.1** and its Python environment. In an activated ESP-IDF shell:

```sh
# Windows
python winbuild.py build --board t_embed

# Linux / macOS
bash build.sh --board t_embed --build-only
```

The board target is `lilygo_t_embed_cc1101` / `esp32s3`. Standard helper builds
write to `build_t_embed/`; `flasher_args.json` there is the source of truth for
that build's flash files and offsets. GitHub Actions are not configured in this repository.
The web flasher is maintained separately and is not included here.

For Dual Boot, download `bruce-dualboot-source-v0.2.0.zip` from the same release,
extract it and use `tools/build_dualboot.py --bruce-dir /path/to/Bruce-Momentum-DualBoot`
in an exported ESP-IDF shell with PlatformIO installed. The archive includes
the complete modified companion source, build configuration and GPL license.

## Limitations and safety

- This is a work-in-progress port, **not full one-to-one Momentum compatibility**.
  Some graphics/asset packs, app APIs and hardware-dependent functions differ.
- U2F has **no secure element** and is not a certified security key. Never use it
  as your only sign-in/recovery method. Erasing NVS or a device failure can lose keys.
- Web-Filesystem uses **unencrypted HTTP**. Authentication does not protect traffic
  from network eavesdropping; use only a trusted network.
- PIN locking is not flash encryption or secure boot. Software and local firmware
  images must be trusted.
- Flipper-encrypted manufacturer keystores cannot be used as on a genuine Flipper.
- ESP-IDF 5.4.1 still requires a security-update review before public release.
  Completed checks are described in [SECURITY-REVIEW.md](SECURITY-REVIEW.md);
  they are not a guarantee that the firmware has no vulnerabilities.

## Credits and licensing

- [Sor3nt / Flipper-Zero-ESP32-Port](https://github.com/Sor3nt/Flipper-Zero-ESP32-Port)
  — ESP32 platform port and application/hardware foundation.
- [Next-Flip / Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware)
  — Momentum code, interface and assets adapted for this fork.
- [BruceDevices / Bruce](https://github.com/BruceDevices/firmware)
  — optional Bruce 1.16 companion. Corresponding modified source and its GPL-3.0
  license are supplied with the Dual Boot release.
- [Flipper Devices](https://github.com/flipperdevices/flipperzero-firmware)
  and the respective application/library contributors.
- [Filip1x3](https://github.com/Filip1x3) — this fork and T-Embed integration.

Original copyright/license notices are retained in source files and libraries.
The Momentum GPL-3.0 license is included in
[licenses/Momentum-Firmware-GPL-3.0.txt](licenses/Momentum-Firmware-GPL-3.0.txt).
