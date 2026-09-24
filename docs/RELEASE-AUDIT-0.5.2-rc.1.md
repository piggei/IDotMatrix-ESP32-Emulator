# 0.5.2-rc.1 Release Audit

**Release:** `0.5.2-rc.1`  
**Build:** `183`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-rc.1-B183`

## Scope

This audit promotes the physically tested `0.5.2-dev / Build 182` baseline to the first 0.5.2 release candidate. No new user-facing runtime feature was added in Build 183.

The runtime-source delta from Build 182 is intentionally limited to:

1. release identity `0.5.2-dev` -> `0.5.2-rc.1`;
2. internal build `182` -> `183`;
3. removal of the unused `IDOTMATRIX_RTC_TYPE_EXPLICIT` bookkeeping macro.

`platformio.ini`, protocol routing, renderers, RTC behavior, buzzer behavior, orientation behavior, BLE behavior and storage logic are otherwise unchanged from the hardware-tested Build 182 baseline.

## Static regression results

All repository static regression tests pass on the RC tree:

- MPU-family / ICM-20689 backend structure and diagnostics;
- BLE notification-descriptor cleanup;
- buzzer configuration;
- ESP32-C3 USB serial / RTC diagnostics structure;
- Clock presentation persistence;
- original-hardware Graffiti full-raster protocol isolation;
- orientation mount mapping;
- preprocessor directive balance;
- RTC configuration/driver structure;
- optional local hardware configuration precedence/preservation.

The release-identity assertion in the Graffiti regression test was updated to `0.5.2-rc.1 / Build 183` without changing its protocol assertions.

## Hardware evidence carried into RC1

The Build 182 baseline immediately preceding RC1 was physically exercised successfully for the new 0.5.2 hardware paths:

- ESP32-C3 + passive buzzer on GPIO3;
- ESP32-C3 + ICM-20689 automatic orientation on the tested shared-I2C configuration;
- ESP32-C3 + DS3231 RTC with the gesture sensor sharing GPIO1/GPIO2;
- battery-backed RTC retention and BLE time writeback;
- cold boot directly into Clock from valid RTC time;
- preservation of Clock style, 12/24-hour mode, date visibility and RGB color across full power removal.

The DS3231 + gesture and ICM-20689 + gesture configurations were tested separately. RC1 does not claim separate physical qualification of DS3231 + ICM-20689 + gesture simultaneously.

## Documentation audit

The RC documentation was normalized so current operational guides describe RC1 behavior rather than the chronology of internal development builds.

Key cleanup actions:

- README, Future Work, hardware configuration/support, orientation and PlatformIO documentation updated for RC1;
- obsolete `0.5.0 / Build 162` release-validation document replaced with the current RC validation gate;
- RC1 release notes added;
- per-build `BUILD-173` through `BUILD-182` notes/audits removed from the public RC package after relevant information was consolidated into `HISTORY.md`, release notes and this audit;
- Wiki current-state pages updated to `0.5.2-rc.1 / Build 183`;
- internal Wiki hardware-image references without corresponding supplied files removed, while the available project demo image is packaged with the Wiki;
- public documentation remains in English;
- historical version/build identifiers remain only where they intentionally describe historical releases/builds.

## Known non-blocking exclusions

- MPU-6050 code path is implemented but not hardware-qualified.
- MatrixPortal + external ICM-20689 is not separately hardware-qualified.
- Password SET/VERIFY remains intentionally unsupported pending complete protocol evidence.
- iOS/RCSP remains an isolated diagnostic/research target.

## Publication status

**Static audit status: PASS.**

Build 183 still requires one final on-device smoke test before publishing RC1 binaries. Because the code delta from the successfully tested Build 182 baseline is limited to release identity plus removal of an unused macro, any observed runtime difference should be treated as a release blocker.

## Archive verification

The generated source and Wiki ZIPs were extracted into clean directories and rechecked. Results:

- regression tests from extracted source archive: **PASS**;
- Markdown/Wiki internal links and packaged image references: **PASS**;
- release identity in extracted source: **PASS**;
- package hygiene (`.pio`, caches, local user config, firmware binaries): **PASS**.
