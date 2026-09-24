# 0.5.0 Release Validation

**Release:** 0.5.0  
**Build:** 162

This document records the qualification scope used to close the 0.5.0 line.

## Hardware-qualified targets

### MatrixPortal ESP32-S3 + 64x64 HUB75

Validated on physical hardware with the official iDotMatrix app, including representative BLE control, storage and display paths.

### ESP32-C3 + 16x16 WS2812

Validated on physical hardware for the native 16x16 profile and WS2812 output path.

## Functional qualification

The following areas are considered qualified for 0.5.0:

- BLE advertising/services and Device Info;
- time synchronization, screen power, brightness and rotation;
- DS3231 RTC boot/read, BLE resynchronization and reboot persistence when the backend is enabled;
- Clock, Countdown, Stopwatch and Scoreboard;
- TEXT rendering and motion modes;
- Solid, Graffiti/DIY, images and GIFs;
- Device Assets Carousel;
- volatile Preset/Default playback;
- Alarm and Program/Schedule multipart media transfers;
- LEVEL and FFT Audio/Rhythm effects;
- dedicated audio framing and recovery to ordinary FA02 commands;
- TEXT/Preset/Carousel display ownership isolation;
- LittleFS-backed persistent media behavior.

## Release constraints

- The classic ESP32 iOS compatibility target is retained as an isolated diagnostic configuration and does not define the qualification status of the main release.
- Password SET/VERIFY remains outside the supported runtime feature set because the complete transaction is not yet characterized.
- Optional future features such as accelerometer-based auto-rotation are intentionally deferred.

## Packaging checks

The final package must contain no generated build output, `.pio` directory, cache directory or temporary diagnostic file. Active release identifiers must be final; historical development identifiers may remain only in the release history. User-facing documentation must remain in English.
