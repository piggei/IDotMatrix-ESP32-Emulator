# Future Work

Release `0.5.2 / Build 190` remains the stable baseline. Active development is `0.6.0-dev.3 / Build 201`. Build 191 exposed the first Waveshare reset-loop problem. Build 192 aligned framework and partition geometry with the qualified WLED target. Build 193 made the Waveshare NimBLE dependency reproducible. Build 194 qualified OTA/captive-portal operation, Build 195 qualified logical 16x16/32x32/64x64 scaling, and Build 196 established the first real memory/GIF baseline. B196 also exposed a `nimble_host` stack-canary during Preset filesystem finalization. B197/B198 moved final Preset/Carousel publication out of `nimble_host` and physically qualified that hardening. B199 then physically qualified transient whole-GIF PSRAM staging on Carousel and live GIF paths. Build 200 added and physically qualified the bounded persistent compressed-source cache including real LRU eviction. Build 201 adds one-item Carousel GIF look-ahead prefetch.

## 0.6.0 development sequence

1. **Build 201** - physically qualify one-item Carousel GIF look-ahead prefetch on the proven B200 cache.
2. **Build 202+** - broader Preset/Carousel transaction hardening, Carousel atomic-bank research and versioned persistence.

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
- Extend PSRAM placement beyond the bounded GIF source cache only when measured benefit and safe reserve policy justify it.
- Continue moving heavyweight work out of latency-sensitive BLE callbacks if profiling identifies a concrete problem.

## Documentation discipline

- Keep user-facing documentation in English.
- Record public release and internal build identifiers separately.
- Treat raw captures as evidence; do not rewrite packet bytes to match a later interpretation.
- Distinguish direct original-hardware observations, emulator policy and inference.
