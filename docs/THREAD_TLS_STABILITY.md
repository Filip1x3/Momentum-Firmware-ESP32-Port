# Wi-Fi / Bluetooth startup reboot fix

The native USB trace captured on 2026-10-21 repeatedly reported
`LoadProhibited` in this call chain:

```
pthread_getspecific -> find_value
wifi_thread_semphr_get_wrapper -> ieee80211_ioctl
coex_update_lpclk_interval -> esp_bt_controller_init
serial_stack_init_once -> bt_srv
```

This is not an SD-card, PIN, icon-layout or asset-pack diagnosis. Furi stored a
`FuriThread*` in FreeRTOS TLS slot 0. ESP-IDF 5.4.1 reserves that same slot for
its pthread-specific linked-list head. Bluetooth controller initialization can
call Wi-Fi coexistence functions from a Furi service thread; pthread then
mistook the task control block for a list and dereferenced an invalid address.

## Fix

- Reserve two usable TLS slots: ESP-IDF pthread at 0, Furi identity at 1.
- Route every Furi identity lookup/store through `core/thread_tls.h`, including
  parent inheritance, current-thread lookup, task reporting and cleanup.
- Reject old one-slot configurations at compile time. Deletion-callback slots
  do not count as usable TLS slots.
- Remove old temporary slot-0 clearing around `pthread_create`; pthread now
  retains ownership of its data and destructors throughout those calls.
- BLE serial startup now returns an NVS initialization error instead of erasing
  shared PIN/U2F storage. This is data-preservation hardening, not the cause of
  the captured panic.

`sdkconfig.defaults` applies to both normal and Dual Boot builds. Existing
build configurations must also set
`CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS=2`; rebuild all components, since
this changes the FreeRTOS static TCB layout. Do not mix old object files with
new headers.

## Regression checks

Run `tools\tests\thread_tls\build.cmd` from the project root in the Windows
build environment. It runs 10,000 identity/POSIX isolation and cleanup cycles
against the production slot-access helper, plus a negative compilation test
for one usable slot with deletion callbacks enabled. These are host mocks,
not an emulation of Wi-Fi or Bluetooth.

Build both firmware profiles. On the physical device verify boot with Wi-Fi
enabled, SD mounted and the lock screen active, then leave it idle beyond the
previous restart interval. Check the serial trace for panics and test button
input. Keep PIN, U2F identity and SD contents intact; no factory reset is needed.

Optional local `MOMENTUM_BOOT_DIAGNOSTICS=ON` logs boot stages on native USB.
It is off by default and does not enable qFlipper or change saved settings.
