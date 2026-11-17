# BLE HAL lifecycle regression test (host-only, no radio)

Run `tools\tests\ble_hal\build.cmd` from the firmware repository root on Windows
with Visual Studio Build Tools. Output is written outside the repository under
the workspace's ignored `.diagnostics/ble-hal-tests`.

This compiles the complete production `ble_spam_hal.c`, actual
`bt_stack_lifecycle.c`, and mechanically extracted, unchanged btshim stop/restore
handlers plus their public message-queue API wrappers. ESP-IDF hardware leaves,
the service queue and settings storage are deterministic stubs. No HAL function
body is copied or reimplemented. The ordinary profile activation leaf is stubbed
and asserts that Bluetooth is enabled in persistent settings before it can run.
The second executable mechanically extracts the current `ble_walk_hal_start` /
`ble_walk_hal_stop` and their lifecycle state declarations, so no scanner, GATT
read/write, or probe implementation is included. Registration completion is a
deterministic platform stub; no peripheral connection is made.

Coverage:

- Failure at each of five initialization steps, cleanup, idempotent stop and
  successful retry.
- 1,000 successful start/stop cycles, alternating BT enabled/disabled in settings.
- Failure at each of four teardown steps, retained ownership, rejection of a
  second initialization while the old stack remains live, and successful retry.
- Combined controller-enable failure plus controller-deinit failure: the partial
  allocation remains owned, blocks a new initialization and is released on retry.
- Refusal to initialize over an incompletely released service-owned stack.
- Repeated start, balanced service records and restoration of the user's disabled
  Bluetooth preference through the **actual** `bt_profile_restore_default` path.

Additional Walk coverage:

- Seven initialization failures plus a missing asynchronous REG completion;
  timeout is bounded and never permits a ready state with `ESP_GATT_IF_NONE`.
- 1,000 start/stop cycles with BT OFF/ON, and already-started calls that perform no
  second initialization or service handoff.
- Valid GATT interface/host checks during teardown and retries, including a host
  already disabled or deinitialized while controller cleanup is incomplete.

Raw advertising configuration and advertising-start stubs unconditionally fail
the test if called. This test constructs no payloads, transmits nothing and is
not a device test or a real ESP-IDF task scheduler.
