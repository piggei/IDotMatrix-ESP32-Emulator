# Build 177 Static Audit

**Release:** `0.5.2-dev`  
**Build:** `177`

## Scope

This audit covers the passive-buzzer implementation introduced on top of the Build 176 post-orientation-qualification baseline.

## Runtime diff boundary

The intended runtime changes are limited to buzzer hardware abstraction and the ESP32-C3 buzzer profile:

- new `NONE`, `ACTIVE` and `PASSIVE` backends;
- passive tone generation through Arduino-ESP32 3.x LEDC;
- ESP32-C3 defaults: GPIO3, 2000 Hz;
- per-event Alarm, Countdown, Schedule and BLE-connection policy defaults on that target;
- local configuration overrides.

No intentional changes were made to BLE protocol parsing, Graffiti, media transactions, Audio/Rhythm, display ownership, storage, orientation classification or display mapping.

## Checks performed

- buzzer profile/configuration regression tests: PASS;
- explicit local `BUZZER_NONE` override against profile defaults: PASS;
- active-backend override against passive profile default: PASS;
- existing ICM-20689/MPU-family static tests: PASS;
- orientation mount mapping tests: PASS;
- local hardware configuration precedence/preservation tests: PASS;
- Graffiti original-hardware raster protocol tests: PASS;
- source documentation relative-link check: PASS.

## Build limitation

PlatformIO is not installed in the audit environment, so a complete Arduino-ESP32 firmware compile/link was not performed here. The passive-buzzer API selection was cross-checked against the Arduino-ESP32 3.x LEDC interface used by the pinned project toolchain. The passive buzzer was subsequently physically qualified on the ESP32-C3 reference hardware.
