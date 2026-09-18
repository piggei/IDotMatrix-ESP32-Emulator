# Hardware Support Status

This document describes the hardware targets currently qualified for the v0.5.x emulator line.

## Support policy

A target is considered **supported** only after end-to-end physical validation of the relevant display output, BLE protocol paths, storage and representative runtime behavior. Compilation or successful boot alone is not sufficient.

## Supported and hardware validated

### Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75

Primary reference target:

- Adafruit MatrixPortal ESP32-S3;
- ESP32-S3 with 8 MB flash and 2 MB PSRAM;
- 64x64 RGB HUB75 panel, 1/32 scan;
- `ESP32-HUB75-MatrixPanel-DMA` backend;
- logical 64x64 iDotMatrix profile;
- LittleFS-backed media storage;
- BLE MTU 517.

The current MatrixPortal S3 path has been validated with the official app across Clock, TEXT, images/GIFs, Carousel, Preset/Default, Alarm, Program/Schedule, timers, Audio/Rhythm, brightness, power and rotation.

The checked-in PlatformIO environment is:

```text
matrixportal_s3_hub75_64
```

### ESP32-C3 + 16x16 WS2812

The native 16x16 ESP32-C3 target is also hardware validated.

Reference PlatformIO environment:

```text
esp32c3_ws2812_16
```

The environment deliberately overrides `lib_deps` so the HUB75 library is not built on this WS2812-only target.

Current reference settings:

- board: `esp32-c3-devkitm-1`;
- logical 16x16 profile (`screenType=0x01`);
- physical 16x16 WS2812;
- matrix data GPIO 4;
- LittleFS using `min_spiffs.csv`.

### Classic ESP32 + WS2812

Classic ESP32 remains compatible with the standalone emulator architecture and was the original standalone platform. It continues to be useful for WS2812 deployments and protocol experiments.

A dedicated environment is retained for the iOS research branch:

```text
ios_compat_esp32_ws2812_32
```

That environment is a diagnostic configuration: logical 32x32, physical 16x16, GPIO17, with iOS handshake experiments enabled. Its iOS compatibility work is not part of the main release gate.

## Logical and physical resolutions

Logical profile and physical panel dimensions are independent. Supported logical sizes are 16x16, 32x32 and 64x64.

The output stage supports:

- 1:1 copy;
- nearest-neighbor upscaling;
- box-average downscaling.

The strongest hardware qualification currently covers the native 64x64 HUB75 target and native 16x16 WS2812 target. Other logical/physical combinations remain useful test configurations but are not all separately hardware-qualified.

## Reference toolchain

The repository PlatformIO baseline uses:

- Arduino-ESP32 3.3.11;
- ESP-IDF 5.5.5;
- pioarduino `platform-espressif32` 55.03.311;
- FastLED 3.10.3;
- AnimatedGIF 2.2.3;
- a pinned `ESP32-HUB75-MatrixPanel-DMA` commit for the HUB75 environment.

See [`PLATFORMIO.md`](PLATFORMIO.md) for build and upload details.
