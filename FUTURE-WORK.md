# Future Work

Stable baseline: **0.6.0 / Build 213**.

The items below are intentionally outside the 0.6.0 release boundary. None is required for the qualified hardware/runtime paths documented in `README.md` and `docs/RELEASE-VALIDATION.md`.

## iOS compatibility research

- Continue the isolated classic-ESP32 RCSP/iOS stimulus experiment only as a separate diagnostic path.
- If iOS still produces no useful AE01/RCSP traffic, prefer a BLE capture from an original iDotMatrix connected to an iPhone over further blind probing.
- Keep iOS-only changes isolated from the qualified Android/official-app runtime.

## Password protocol

Password support remains intentionally disabled because the complete transaction and enforcement behavior are not fully established.

- Determine the exact SET-password completion exchange expected by the official app.
- Determine whether authentication enforcement is device-side, app-side or shared.
- Add runtime password handling only after the complete transaction is supported by direct evidence.

## OTA hardening

The Waveshare maintenance OTA path is hardware-qualified for successful upload, interrupted-upload recovery to the previous firmware, and persistence of stored state. Automatic rollback after a fully written but non-bootable image is **not** claimed.

- Evaluate explicit post-boot validation/rollback only if that additional failure mode needs to be covered.
- Keep OTA physically triggered and Wi-Fi disabled during normal runtime.

## Original-hardware research

- Complete enclosure/PCB photographs, connector mapping, dimensions and power-path notes for the original 64x64 device.
- Precisely characterize the original Program buzzer duration if useful for fidelity documentation.
- Verify which additional original-device settings survive or reset across `03/80` if exact fidelity becomes important.
- Capture additional original-device TEXT marker/record variants only where stronger protocol evidence is useful.

## Possible platform extensions

- Hardware-qualify the supplied GY-521 / MPU-6050 candidate by confirming runtime `WHO_AM_I` and exercising the existing MPU-6050 orientation path.
- Hardware-qualify the MatrixPortal + external ICM-20689 profile if that exact combination is needed.
- Add further accelerometer backends only through the normalized X/Y/Z interface and common orientation engine.
- Evaluate additional RTC backends only when real hardware is available; keep the qualified DS3231 path unchanged unless new evidence requires it.
- Evaluate the Waveshare on-board PCF85063 RTC, QMI8658 IMU, SHTC3, MicroSD and audio hardware as separate features; none is enabled or claimed by 0.6.0.
- Qualify additional logical/physical scaling combinations beyond the currently exercised reference paths when matching hardware is available.
- Extend PSRAM placement beyond the bounded GIF source cache only when measured benefit and reserve policy justify it.
- Continue moving heavyweight work out of latency-sensitive BLE callbacks only when profiling identifies a concrete need.

## Documentation discipline

- Keep user-facing documentation in English.
- Record public release and internal build identifiers separately.
- Treat raw captures as evidence; do not rewrite packet bytes to fit a later interpretation.
- Distinguish direct original-hardware observations, emulator policy and inference.
