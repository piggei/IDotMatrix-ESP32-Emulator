# Future Work

Release `0.5.2-rc.2 / Build 184` is the current release-candidate baseline. The items below are non-blocking research or possible future extensions and are not RC2 release blockers. The latest stable public release remains `0.5.1 / Build 172` until the 0.5.2 line is promoted to final.

## iOS compatibility research

- Evaluate the isolated classic-ESP32 RCSP stimulus experiment.
- If iOS still produces no AE01/RCSP traffic, prefer a BLE capture from an original iDotMatrix connected to an iPhone over further blind probing.
- Keep iOS-only changes isolated from the qualified main runtime.

## Password protocol

Password support remains intentionally disabled because the complete transaction is not understood.

- Determine the exact SET-password completion exchange expected by the official app.
- Determine whether authentication enforcement is device-side, app-side or shared.
- Add runtime password handling only after the transaction is supported by direct evidence.

## Original-hardware research

- Photograph the original 64x64 enclosure, matrix, PCB front/back, connectors and wiring.
- Record readable IC/PCB markings, regulators, memory devices and test/programming pads.
- Add measured board/enclosure dimensions and power-path notes to `docs/ORIGINAL-HARDWARE-64X64.md`.
- Precisely characterize the original Program buzzer duration if useful for protocol documentation.
- Verify which additional original-device settings survive or reset across `03/80` if exact fidelity becomes important.
- Capture the exact original-device marker/record variant for every 64-pixel TEXT glyph family if stronger protocol evidence is desired.

## Possible platform extensions

- Hardware-qualify additional ICM-20689 board/wiring combinations only where they materially differ from the qualified ESP32-C3 shared-I2C setup.
- Hardware-qualify the implemented MPU-6050 code path if a genuine MPU-6050 module becomes available.
- Add further accelerometer backends only through the existing normalized X/Y/Z interface and common orientation engine.
- Evaluate additional RTC backends only when real hardware is available; keep the hardware-qualified DS3231 path unchanged unless new evidence requires it.
- Hardware-qualify additional logical/physical scaling combinations beyond native 64x64 and native 16x16.
- Revisit PSRAM placement only if future media sizes or features demonstrate a real need.
- Continue moving heavyweight work out of latency-sensitive BLE callbacks if profiling identifies a concrete problem.

## Documentation discipline

- Keep user-facing documentation in English.
- Record public release and internal build identifiers separately.
- Treat raw captures as evidence; do not rewrite packet bytes to match a later interpretation.
- Distinguish direct original-hardware observations, emulator policy and inference.
