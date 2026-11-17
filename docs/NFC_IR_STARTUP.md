# NFC result parsing and Universal IR startup

## NFC

Read/Detect retain the earlier deferred-plugin change: detecting and reading a
card never maps supported-card parser ELFs first. The parser cache now lists
filenames without mapping executable code just to discover protocol/features.
When the card result actually needs parsing, plugins are tried in directory
order until one accepts it. Their discovered metadata is cached per NFC app;
later reads/parses skip known incompatible protocols and missing operations.
Unknown/additional plugins remain supported and are inspected on demand.

The filename iterator also skips directories and unrelated files rather than
mistaking the first README/short filename for the end of the plugin directory.
Temporary load contexts are freed after every operation and cleared to NULL.

This is not a new NFC poller or a PN532 timing change. If no parser recognizes
the card, the first parse still has to inspect all candidates. Later matching
attempts benefit from cached metadata. There is no persistent SD metadata cache
to become stale after replacing the card or the pack.

## Universal IR, including TVs

Opening the remote builds only a name/offset index using the existing buffered
reader. It no longer decodes every protocol body or allocates every RAW timings
array before displaying the buttons. The selected signal is decoded and
validated immediately before transmission using the original transmit path.
Strict per-record key order prevents missing fields from being borrowed from
the following named record. Unused button names are ignored, not asserted.
Rebuilding the index clears previous offsets first, avoiding duplicate entries
after retries. SD I/O errors remain errors.

The underlying database is still read to find the record names, so this is not
an on-device promise of a fixed startup duration. IR carrier, duty-cycle policy,
signal encoders, repeat counts and transmit timing have not been changed.
Malformed bodies are now rejected when selected, not during menu construction.

## Verification

From the repository root on the Windows development machine:

```text
tools\tests\nfc_ir\build.cmd
python -m unittest discover -s tests -p test_nfc_ir_startup.py -v
python -m unittest discover -s tests -p test_startup_paths.py -v
```

The native harness mechanically extracts current production function bodies.
Fake Storage/ELF/IR boundaries check filename iteration, no ELF mapping while
building the NFC cache, early matching, reuse of protocol/feature metadata,
balanced load contexts, 1,000 IR index rebuilds, exact offsets, unused names,
file-open/I/O failures, out-of-range indices and refusal to transmit on seek,
decode or validation failure. It never transmits radio/IR or polls NFC hardware.
The two new source guards and eight existing startup-wiring checks also pass.

Both real firmware profiles pass a complete build. Physical checks after
flashing still need to compare NFC detection/result time and opening TVs on the same
SD card, then verify a known remote on owned equipment. Host operation counts
do not measure hardware time or certify card/TV compatibility.
