# Asset-pack stability review — 2026-10-21

## Report and limits of the diagnosis

A user reported repeated restarts after installing an asset pack and pressing
Reset on a T-Embed CC1101 Plus. The supplied 10-second video shows a desktop,
black screen, Momentum startup screen, and desktop again. It does not include a
serial panic/backtrace, firmware-image checksum, or the exact SD contents.
The pack was reported as “hunter”, interpreted for testing as Momentum's Haunter.

This review fixes reproducible code defects. It does **not** establish which one
caused the reported restart, and it is not a physical regression test on that Plus.
It does not imply complete Plus/nRF24 support or certify the whole firmware.

## Fixes

- Changing packs now saves settings and restarts. The old live-reload path freed
  icon/font allocations while existing GUI/menu objects could still reference them.
- The asset service builds its icon/font set privately and publishes it only when
  complete, rather than exposing a list being modified to GUI readers.
- Static menu/FAP icons (one frame, no frame rate) no longer start an animation
  timer or trigger its nonzero-rate assertion. Animated timer periods are at least
  one tick.
- File graphics have explicit logical-canvas dimension, file-size, encoded-header
  and exact decompressed-length checks. The GUI receives validated raw frames.
  Invalid resources fall back to built-in graphics instead of reaching the renderer.
- Replacements larger than their original icon box are downscaled proportionally
  in RAM; normal-size graphics are unchanged. No file in the downloaded pack is edited.
- Static BMX resources can replace an originally animated icon. This previously
  looked only for an animation directory and silently missed static replacements.
- Icon/font cache data has a 512 KiB budget; allocation failures and partial loads
  release their allocations and keep the original resource.
- Custom fonts validate record offsets, Unicode lookup landings and bounded glyph
  bit streams before u8g2 sees them. Invalid fonts fall back to built-in fonts.
- Animation metadata is validated before narrowing integer fields. Zero passive
  frames/rates, oversized dimensions/counts, invalid frame orders and incomplete
  bubble records are rejected. Animation names/manifest counts/weights are bounded.
- Failed animation loads free frames even if failure occurs after loading frames;
  partial cleanup is safe to repeat. Incomplete bubble chains are also freed.
- Menu and desktop-animation timers are stopped/flushed before their views and
  callback contexts are freed. Pending activation state is reset on animation change.
- Generic raw decompression copied one byte too many (including the marker in the
  copy length). It now copies only payload bytes, and rejects truncated headers.

The Wi-Fi service deliberately draws two short marks when enabled but not connected
to a network. That state alone is not evidence of corrupted graphics.

## Verification

`tools/tests/assets/build.cmd` builds the real production asset parser and heatshrink
decoder as a host library, plus an executable using the real icon-animation and
generic compression sources with test timer stubs.

`tools/test_asset_files.py --library <host library> --pack <original ZIP>` covers
raw/compressed data, exact lengths, non-byte-aligned rows, dimension limits,
too-small output buffers, guarded output boundaries, scaling, names, font structure,
and 5,000 seeded malformed inputs. Pack tests read ZIP members without extraction.
The host lifecycle executable exercises 1,000 static-icon start/stop cycles,
animated timer bounds, raw-copy canaries and truncated compression headers.

Original upstream packs used locally:

- [Haunter](https://github.com/Next-Flip/Asset-Packs/tree/dev/haunter): 110 animation
  frames, 13 BMX icons, no fonts.
- [Swirl](https://github.com/Next-Flip/Asset-Packs/tree/dev/swirl): 17 animation frames.
- [Pirates](https://github.com/Next-Flip/Asset-Packs/tree/dev/pirates): 521 animation
  frames, 9 BMX icons, 3 fonts.

All these resources pass the native parser tests. This checks file parsing, not
every graphical layout or interaction on hardware. No assertion is made that every
community asset pack works.

## Before releasing

On a real device, test Default → Haunter → Default, including the automatic restart
on leaving settings; reboot with each pack selected; repeatedly open/close the main
menu and Control Center; rotate rapidly in all menu styles; open a static FAP menu
shortcut; repeat with Wi-Fi enabled and disconnected/connected. Test both normal and
paired profiles without crossing their partition layouts.

If the reporter still sees a restart, obtain the exact firmware version/checksum,
original installed pack archive, and USB boot/panic log. Resetting or formatting the
SD card/NVS is not part of this fix. Preserve U2F credentials and user files.
