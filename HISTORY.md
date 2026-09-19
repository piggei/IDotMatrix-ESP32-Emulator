# Development history

## 0.6.0-dev / Build 165

- Consolidated the LIS3DH backend and common orientation support layer.
- Added `IDOTMATRIX_ACCEL_MOUNT_ROTATION` with compile-time validation for `0`, `90`, `180` and `270` degrees clockwise.
- Mount compensation is sensor/board agnostic, allowing the same accelerometer backend to be reused on custom boards or as an external module without changing the orientation engine.
- MatrixPortal S3 explicitly uses a `0` degree mount offset, preserving the hardware-qualified Build 164 behavior.
- No renderer, BLE protocol, media, audio, automation or display-mode behavior changed.

# Release History

## 0.6.0-dev / Build 164

First automatic-orientation build.

- qualified the MatrixPortal S3 physical axis map from Build 163 measurements: `+Y`=0 deg, `+X`=90 deg clockwise, `-Y`=180 deg and `-X`=270 deg clockwise;
- enabled automatic display rotation through the common orientation engine;
- applied rotation only at the final logical-to-physical output stage so all existing renderers remain unchanged;
- static framebuffers are refreshed immediately when a stable orientation changes;
- retained the existing app-controlled 180-degree flip as a separate output transform;
- reduced normal orientation logging to initialization and actual rotation changes;
- kept detailed X/Y/Z sample diagnostics available behind a separate compile-time define;
- ESP32-C3 and classic ESP32 remain free of accelerometer code unless an `IDOTMATRIX_ACCEL_DRIVER_*` backend is selected.

## 0.6.0-dev / Build 163

First orientation-sensor development build.

- added compile-time accelerometer backend selection;
- selecting a supported `IDOTMATRIX_ACCEL_DRIVER_*` automatically enables the common orientation subsystem;
- added the MatrixPortal S3 LIS3DH backend at I2C address `0x19`;
- added normalized X/Y/Z sampling, dominant-axis classification, hysteresis and stable-direction timing;
- added Serial diagnostics for the four physical panel orientations;
- display rotation is intentionally not applied yet;
- ESP32-C3 and classic ESP32 targets remain free of accelerometer dependencies unless a driver is explicitly enabled.

The runtime behavior outside the new diagnostic orientation subsystem is unchanged from the 0.5.0 baseline.

## 0.5.0 / Build 162

Final 0.5.0 release of the standalone iDotMatrix ESP32 Emulator.

Highlights:

- hardware-qualified Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75 target;
- hardware-qualified ESP32-C3 + 16x16 WS2812 target;
- logical 16x16, 32x32 and 64x64 display profiles with resolution-independent output scaling;
- Clock, Countdown, Stopwatch and Scoreboard rendering aligned with original-device references;
- TEXT support for 16/32/64-pixel glyph families, scrolling, paging and multiline static layouts;
- image/GIF playback, Graffiti/DIY, persistent Device Assets Carousel and volatile Preset/Default playback;
- Alarm and Program/Schedule multipart media handling with CRC32 validation;
- five LEVEL and five FFT Audio/Rhythm effects with dedicated audio stream framing;
- verified TEXT/Preset/Carousel display ownership isolation;
- LittleFS media storage and transactional replacement paths;
- cleaned public documentation and release metadata.

The 16x16 Stopwatch animation was revalidated frame-by-frame against the supplied original-device reference video before this release. No additional visual change was required.

## 0.4.0 / Build 118

Previous stable standalone emulator release. It established the original WS2812-focused baseline before the larger-display, multipart-media, Preset and MatrixPortal/HUB75 work included in 0.5.0.
