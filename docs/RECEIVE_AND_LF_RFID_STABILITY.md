# Receive lifecycle and LF RFID hardware capability

## 125 kHz RFID is not built into T-Embed CC1101

The stock board has a PN532 for 13.56 MHz NFC, not a 125 kHz analog reader.
The manufacturer explicitly documents this distinction in its
[hardware repository](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101).
The [board documentation](https://wiki.lilygo.cc/products/t-embed-series/t-embed-cc1101/)
also lists PN532 rather than a low-frequency reader.

The previous board definition incorrectly enabled the optional RDM6300 UART
backend by default. With no module attached it kept waiting for UART data,
looking like a broken built-in reader. It also reassigned GPIO44/43, which are
shared with the optional nRF24 CS/CE connection.

The default build now reports `furi_hal_rfid_is_supported() == false`. Read and
RAW Read show a hardware-required screen without starting the capture worker.
The existing external RDM6300 read-only backend remains opt-in for a custom,
properly wired build (`BOARD_HAS_RFID=1`). This reports a build capability, not
proof that a module is electrically connected. The shared expansion pins must
not be used simultaneously for RDM6300 and nRF24.

No decoding, writing, emulation, or access-control capability was added.

## ProtoPirate passive receive stability

The live receiver had several independent lifetime/concurrency hazards:

- Its decoder worker modified the history arena and shared scratch
  `FlipperFormat` concurrently with GUI drawing, deletion, and deferred storage.
- The worker sent a blocking GUI event. A full dispatcher queue could leave the
  GUI joining a worker that was itself waiting for the GUI to drain that queue.
- Registry cleanup freed the receiver before joining a partially started worker.
- Application shutdown could free history while its receiver view still held the
  pointer for a final GUI draw.

History operations shared by live RX now use the receiver model's existing
mutex, the same guard already used for GUI draw/input. No second lock is nested,
and the opaque history layout remains unchanged for existing file-decoder FALs.
The dirty flag uses existing application alignment padding, checked at compile
time to preserve plugin offsets. The decoder callback does not queue GUI events
or nest view method calls: it marks history dirty, and the existing 100 ms GUI
tick coalesces updates. Async RX stops before
worker join; worker callbacks are finished before the receiver is freed. The
view drops its history pointer before that history is destroyed.

These changes do not modify radio presets, protocol decoders, transmitters,
rolling-code handling, emulation, or brute-force tools.

## Verification

`tests/test_receive_rfid.py` checks five wiring contracts covering the board
default/override, unsupported read exits, history synchronization, no worker
GUI queue writes, and cleanup ordering.

`tools/tests/receive_rfid/build.cmd` compiles the actual production callback and
teardown functions extracted from source. A native non-radio harness runs
100,000 concurrent capture/delete/read iterations and 20,000 active/partially
started teardown cycles with lifetime assertions. It also compiles the actual
capability function against the stock board header both with its default and
with the explicit external-reader override.

These host tests do not replace a physical receive test. In particular, they do
not demonstrate that a stock T-Embed can read a 125 kHz tag without extra hardware.
