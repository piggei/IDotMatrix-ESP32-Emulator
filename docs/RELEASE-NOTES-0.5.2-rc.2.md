# iDotMatrix ESP32 Emulator 0.5.2-rc.2

**Release:** `0.5.2-rc.2`  
**Build:** `184`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-rc.2-B184`

## Purpose

RC2 is a narrowly scoped correction to the first 0.5.2 release candidate. It changes only the boot-display selection order after persistent state has been loaded.

## Boot-display priority fix

The boot policy is now:

1. resume the first valid persisted Device Assets/Carousel slot;
2. if no stored Carousel can start, use a valid DS3231 RTC to start Clock;
3. otherwise leave the screen off.

This ensures the optional RTC extension does not override a user-configured persistent Carousel. Clock presentation persistence (style, 12/24-hour mode, date visibility and RGB colour) remains unchanged and is used whenever the RTC-backed Clock is selected.

## Unchanged areas

No other runtime behavior is intentionally changed from RC1/B183. BLE protocol handling, Graffiti, normal Bulk, media storage, Audio/Rhythm, alarms/schedules, RTC retry/recovery, orientation backends, buzzer support and hardware configuration remain unchanged.

## Validation

A dedicated static regression test locks the order `Carousel -> RTC Clock -> screen off`. The full repository static suite must also pass before packaging. A short on-device smoke test should verify both Carousel-present and Carousel-absent cold boots.
