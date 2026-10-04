# Future Work

Release `0.5.2 / Build 190` remains the stable baseline. Active development is `0.6.0-dev.3 / Build 194`. Build 191 exposed the first Waveshare reset-loop problem. Build 192 aligned the framework and partition geometry with the qualified WLED target, then failed the compile gate because that stack does not provide the legacy Arduino BLE compatibility headers. Build 193 added the Waveshare-only NimBLE-Arduino 2.5.1 backend while preserving the legacy BLE path for existing qualified profiles. Build 194 hardens the already-present OTA maintenance AP with captive-portal discovery and redirects while keeping the firmware upload path unchanged.

## 0.6.0 development sequence

1. **Build 194** - qualify the existing Waveshare bring-up and OTA path with captive-portal discovery; verify clean PlatformIO build, stable boot, native 64x64 HUB75, BLE, automatic portal opening, direct-IP fallback and interrupted-upload recovery.
2. **Build 195** - qualify 16x16/32x32/64x64 logical scaling on the 64x64 Waveshare panel.
3. **Build 196** - measurement-only PSRAM/heap baseline and media-latency telemetry.
4. **Build 197** - guarded whole-file GIF staging in PSRAM with LittleFS fallback.
5. **Build 198** - bounded persistent GIF source cache.
6. **Build 199** - one-item Carousel look-ahead prefetch.
7. **Build 200+** - Preset transaction hardening, Carousel transaction research/atomic-bank work and versioned persistence.

Waveshare on-board PCF85063, QMI8658, SHTC3, MicroSD and audio devices are intentionally deferred until the board/display/BLE/OTA baseline is physically qualified.

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
- Hardware-qualify the supplied GY-521 / MPU-6050 candidate by confirming its runtime `WHO_AM_I` and exercising the existing MPU-6050 orientation path on real hardware.
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
