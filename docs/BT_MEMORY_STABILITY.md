# Radio memory and application-launch stability

The 2026-10-21 on-device follow-up to the TLS fix found a second problem.
The idle desktop no longer hit the pthread panic, but launching Sub-GHz could
assert in `furi_mutex_alloc -> xSemaphoreCreateMutexStatic` because internal
allocation returned NULL. The loader reported only 6,452 free internal bytes.

The native USB trace showed Wi-Fi starting first, then the BLE serial profile
starting from a default-profile refresh despite Bluetooth being disabled.
`namechanger_on_system_start` requests that refresh after loading the device
name. An SD settings reload could also start the serial profile unconditionally.

## Changes

- Default-profile restoration is now a distinct request handled on the BT
  service thread. It reloads settings and keeps Bluetooth off if disabled;
  changing a device name is not authorization to enable the radio.
- An SD settings/key reload also respects disabled Bluetooth.
- Radio shutdown no longer trusts `BtStatusOff`: that status can mean merely
  that advertising stopped while the controller still owns memory.
- `bt_stack_deinit` shuts down Bluedroid before its controller and handles both
  enabled and initialized states. Controller disable leaves `INITED`, and deinit
  must run from that state; the old `IDLE` check skipped deinit and leaked its
  allocations until reboot.
- API errors stop the teardown and report unavailable rather than falsely
  claiming that all resources were released.
- No irreversible `esp_bt_mem_release(ESP_BT_MODE_BLE)` is used. Bluetooth can
  subsequently be enabled again. No settings, bonds, PIN or U2F keys are erased.

Run `tools\tests\bt_lifecycle\build.cmd` for native tests of the production
teardown helper. Mock controller/host states cover all nine initial state
combinations, repeated shutdown, four failure/retry paths, invalid states and
10,000 restartable cycles. These tests do not emulate the radios or GUI.

Physical validation must additionally check Wi-Fi-on boot with SD and lock,
application entry/exit, and switching Bluetooth/Wi-Fi. The TLS fix in
`THREAD_TLS_STABILITY.md` is also required; this memory fix does not replace it.
