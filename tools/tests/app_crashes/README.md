# Caesar Cipher / Bomberduck regression checks

Run `tools\tests\app_crashes\build.cmd` from the firmware repository root on
Windows with the existing Visual Studio Build Tools installation. This compiles
the **actual application C files**, not rewritten copies, against strict host
stubs. Output goes to the workspace's ignored `.diagnostics/app-crash-tests`.

The test runs 1,000 complete entry/input/exit cycles per app and checks:

- Caesar Cipher frees its mutex exactly once and never while held, registers
  zero-initialized text, and computes all 25 rotations for a 255-character input.
- Bomberduck owns its mutex on 66,000 timeout-driven simulation ticks, releases
  it before Back/exit, initializes the world before the first possible draw,
  and detaches GUI/input callbacks before freeing their queue or mutex.
- GUI draws skip a busy game update without waiting, queued input never blocks
  the shared GUI task, and enemy initialization cannot overrun its fixed array.

This is a lifecycle/regression test, not a physical-device gameplay test or an
ESP32 emulator. Both FAPs must also be rebuilt with `buildFap.sh` and its symbol
check, then replaced on the SD card; flashing firmware alone leaves old FAPs
unchanged.
