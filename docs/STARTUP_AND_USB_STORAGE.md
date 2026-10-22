# Application startup and USB Storage handoff

## Startup changes

- Sub-GHz loads `setting_user`, the keystore, and `last_subghz.settings` through
  the existing 1 KiB buffered stream. Settings values, file formats, fallback
  defaults, radio setup, and RF timings are unchanged. The buffer is freed with
  each reader; it is not a persistent copy that could outlive an SD swap.
- NFC Detect/Read no longer map all supported-card parser plugins before
  starting the scanner/poller. Protocol support used for reading is built in.
  Supported-card parsers load lazily when a result is parsed, then reuse the
  existing per-application cache. Saved-card paths still work. The first result
  can still spend time loading plugins; this does not accelerate card-level
  communication or change PN532 timeouts.

## USB Storage

The request from Settings is placed on the Desktop event queue. If a Loader
application is still running, Desktop waits for its completion event, not a
400 ms polling timer. It opens a foreground full-screen ViewHolder above the
still-existing Settings/menu windows. Input belongs to that screen.

Before handing over SD, Desktop freezes its SD-backed animation, inhibits
automatic locking, and reserves Loader. The active FAM storage service checks
open files and soft-unmounts FATFS; only a successful unmount may continue into
MSC. A HAL filesystem release then synchronizes its state without freeing the
initialized card or SPI device needed for raw access. Failure/denial must not
be ignored. The unused POSIX Storage shim is not the active firmware backend.

On exit, MSC is stopped first and the host-removal grace period is preserved.
Only then is SD remounted, the USB composite released, and the animation and
normal input restored. Failure paths only unwind stages actually entered.
The operation does not format SD or write PIN/U2F/NVS data. Users must still
eject the drive on the computer before disconnecting it on the device.

## Verification

Run from the repository root on the Windows development machine:

```text
tools\tests\startup\build.cmd
python -m unittest discover -s tests -p test_startup_paths.py -v
```

The native test compiles the production buffered reader/cache with counted,
memory-backed storage. It checks EOF, missing/short files, cache boundaries,
rewinds, 10,000 seek/read operations, and absence of writes. Three byte-at-a-time
passes over 8,193 bytes use 30 underlying reads rather than 24,579. This is an
I/O-count test, not a measured on-device startup-time claim.

The Python checks verify source wiring/order for NFC loading and the USB
handoff. A second native test mechanically extracts the production Desktop/FAM
storage handoff functions and runs them against a fake hardware layer: 1,000
SD-to-MSC-to-SD cycles, open-file denial, and unmount/release/install/start/remount
failures. It checks that the card stays initialized and that SD is never
remounted while MSC is active. Both firmware profiles must also compile. These tests do not certify
physical NFC detection or USB host enumeration: after flashing, verify Read,
Sub-GHz open/back, and Settings > System > Others > USB Storage; safely eject
the card, leave USB Storage, and check that apps can read SD again.
