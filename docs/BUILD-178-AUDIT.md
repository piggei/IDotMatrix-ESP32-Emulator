# Build 178 Static Audit

**Release:** `0.5.2-dev`  
**Build:** `178`

## Scope

This audit covers the DS3231 RTC integration added on top of the hardware-qualified Build 177 ESP32-C3 baseline.

## Runtime diff boundary

Intended runtime changes are limited to:

- DS3231 direct-I2C backend;
- common RTC configuration macros;
- ESP32-C3 shared-I2C profile defaults on GPIO1/GPIO2;
- RTC-based persistent time source selection;
- BLE-to-RTC time synchronization;
- DS3231/MPU-family `0x68` collision protection;
- RTC-aware restore-to-Clock behavior after Alarm/Schedule activity.

No intentional changes were made to Graffiti framing, generic Bulk, Audio/Rhythm, media transactions, framebuffer mapping, ICM-20689 register configuration, orientation classification or passive-buzzer waveform generation.

## Checks performed

- DS3231 configuration/profile tests: PASS;
- direct RTC driver syntax check with strict warnings: PASS;
- direct driver verified not to call `Wire.begin()`: PASS;
- explicit DS3231 + MPU-family `0x68` conflict rejection: PASS;
- MPU-family auto-probe remains available with DS3231: PASS;
- existing ICM-20689/MPU-family tests: PASS;
- buzzer configuration tests: PASS;
- orientation mount tests: PASS;
- Graffiti original-hardware raster tests: PASS;
- local hardware configuration precedence/preservation tests: PASS.

## Qualification boundary

The DS3231 backend is implemented and statically validated but is not yet marked hardware-qualified in the emulator. Physical verification should confirm boot detection, displayed time, BLE synchronization, reboot persistence and coexistence with the other devices on GPIO1/GPIO2.
