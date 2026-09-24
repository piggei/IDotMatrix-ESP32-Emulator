# Build 180 Static Audit

## Scope

Build 180 is derived from Build 179 and is intentionally limited to ESP32-C3 serial routing, startup diagnostics, version metadata and documentation.

## Findings

- The DS3231 driver already clears OSF in `adjust()` after writing a valid BLE-synchronized date/time.
- `applyBootDisplayPolicy()` is called after RTC initialization and display initialization; therefore a non-Clock cold boot indicates that RTC readiness or validity is false at that point, or that RTC support was not compiled into that build.
- Arduino IDE builds do not inherit `platformio.ini` `IDOTMATRIX_DEFAULT_*` settings. A local user config is therefore required to reproduce the PlatformIO C3 hardware profile outside PlatformIO.
- The ESP32-C3 PlatformIO profile previously lacked the native USB CDC/JTAG build flags required for Arduino `Serial` output on a native-USB-only board.
- The `esp_core_dump_flash: Core dump data check failed` boot message is emitted by ESP-IDF core-dump integrity checking and is separate from the emulator RTC path.

## Runtime boundary

No intentional changes were made to DS3231 register access, BLE time synchronization, Alarm/Schedule timing, display rendering, buzzer behavior or BLE protocol handling.

## Static verification

- all repository Python regression tests pass;
- preprocessor-balance regression passes;
- C3 native-USB flags and repeated startup-summary markers are covered by `tests/test_c3_serial_diagnostics.py`;
- all local Markdown links in the source repository resolve;
- the Wiki retains four pre-existing hardware-image references whose image files are not present in the supplied Wiki archive; no new missing Wiki targets were introduced by Build 180;
- `IDotMatrixUserConfig.h` is not shipped in the source archive.
