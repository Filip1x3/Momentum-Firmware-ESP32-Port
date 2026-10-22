# Flashing Momentum ESP32 Port v0.2.0

For LilyGo T-Embed CC1101 only. Back up SD and important device data first.

Use the [Web Flasher](https://momentum-esp32-port.pages.dev/) with desktop Chrome
or Edge. Choose **Dual Boot Off** for Momentum only, or **On** for Momentum + Bruce.
The website also creates the matching installer ZIP when you click Download.
GitHub release attachments contain only the separate BIN files and sdcard.zip.

## Exact serial offsets — standard variant

| File | Hex offset |
|---|---|
| bootloader.bin | 0x0 |
| partition-table.bin | 0x8000 |
| furi_esp32.bin | 0x10000 |
| ota_data_initial.bin | 0xBF0000 |

## Exact serial offsets — Dual Boot variant

| File | Hex offset |
|---|---|
| bootloader-dualboot.bin | 0x0 |
| partition-table-dualboot.bin | 0x8000 |
| momentum-dualboot.bin | 0x10000 |
| bruce-dualboot.bin | 0x600000 |
| ota_data_initial-dualboot.bin | 0xBF0000 |

Flash settings: ESP32-S3, 16 MB, DIO, 80 MHz. Use all matching files in one
segmented write operation for first installation or layout migration.
Do not mix the two variants. ota_data_initial.bin never belongs at 0x10000.
Do not erase the whole chip or use a merged/factory image: that can destroy
NVS/U2F credentials. If a reset leaves download mode selected, restart without BOOT.

Standard-to-standard web updates use the inactive OTA slot. Any transition
involving Dual Boot needs the complete matching package. Both Dual Boot slots
hold systems, not spare update space. A paired installation is not power-loss-atomic;
keep USB connected throughout and retain recovery access. Raw BIN uploads cannot
enable or disable Dual Boot in the web flasher.

Extract sdcard.zip contents to a FAT32 SD-card root, keeping apps and apps_assets.
Back up existing files first. The browser does not automatically write the SD card.
Bruce requires writable SD and keeps its files under /bruce. Return through
Momentum Port → Go back to Momentum Port. Do not flash stock Bruce into either slot.

Complete corresponding modified Bruce source and GPL license:
[source archive](../third_party/bruce-dualboot/bruce-dualboot-source-v0.2.0.zip).
Dual Boot remains experimental; not every browser installation path or Bruce
feature has been physically tested. Keep another account recovery method for U2F.
