# Future Work

The 0.5.0 release is feature-complete for its qualified hardware targets. The items below are non-blocking research or possible future extensions.

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

- MatrixPortal S3 LIS3DH automatic orientation is hardware-qualified; Build 165 also adds generic planar mount compensation for custom/external sensor installations.
- Add and qualify the MPU-6050 / GY-521 backend using the same common orientation engine.
- Add the external GY-521 / MPU-6050 backend using the same common orientation engine.
- Revisit optional DS3231 RTC integration on the MatrixPortal S3 platform.
- Hardware-qualify additional logical/physical scaling combinations beyond native 64x64 and native 16x16.
- Revisit PSRAM placement only if future media sizes or features demonstrate a real need.
- Continue moving heavyweight work out of latency-sensitive BLE callbacks if profiling identifies a concrete problem.

## Documentation discipline

- Keep user-facing documentation in English.
- Record public release and internal build identifiers separately.
- Treat raw captures as evidence; do not rewrite packet bytes to match a later interpretation.
- Distinguish direct original-hardware observations, emulator policy and inference.
