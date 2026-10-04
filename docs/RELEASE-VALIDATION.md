# 0.6.0 Release Validation

**Release:** `0.6.0`  
**Build:** `213`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-B213`

Build 213 is the stable promotion of the hardware-qualified Build 212 runtime. The stable promotion changes release/build identity and documentation only; it does not introduce new protocol, media, storage or scheduling behavior.

## Qualified reference targets

### Waveshare ESP32-S3 RGB Matrix + 64x64 HUB75

Hardware-qualified in the 0.6.0 line for:

- native 64x64 HUB75 output;
- logical 16x16, 32x32 and 64x64 profiles on the physical 64x64 panel;
- BLE operation with NimBLE-Arduino;
- physically triggered OTA, successful update and interrupted-upload recovery;
- persistence of stored state across OTA interruption;
- GIF PSRAM staging, bounded compressed-source cache and Carousel prefetch;
- deferred filesystem publication outside `nimble_host`;
- crash-recoverable whole-Carousel-bank replacement;
- invocation-atomic live-session Preset replacement;
- static PNG Device Assets plus animated GIF and TEXT items;
- TEXT-to-next-slot Carousel continuation;
- repeated Carousel replacement with the Waveshare NimBLE host-task stack set to 8 KiB and no reproduced stack-canary/reboot in the qualification run.

### Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75

Carries forward the established hardware-qualified large-panel baseline, including representative BLE control, media/storage, Clock/TEXT, timers, automation, Audio/Rhythm and LIS3DH automatic orientation.

### ESP32-C3 + 16x16 WS2812

Carries forward the 0.5.2 hardware-qualified compact baseline, including DS3231 persistent timekeeping, passive low-level-trigger buzzer support, autonomous Alarm/Schedule behavior and the qualified external ICM-20689 orientation configuration.

## Functional scope

The stable release includes:

- BLE advertising, services, characteristics and Device Info;
- time synchronization, screen power, brightness and app-controlled flip;
- Clock, Countdown, Stopwatch and Scoreboard;
- TEXT static/multiline and scrolling modes;
- Solid, live Graffiti/DIY, full-raster Graffiti, static image and GIF;
- persistent Device Assets Carousel and volatile Preset/Default;
- Alarm and Program/Schedule multipart media;
- LEVEL and FFT Audio/Rhythm framing and effects;
- LittleFS media ownership and persistence rules;
- automatic orientation at the final logical-to-physical output stage;
- deterministic boot priority: persisted Carousel, then valid RTC Clock, otherwise screen off.

## Explicit exclusions

The following are not release blockers and are not claimed as qualified by 0.6.0:

- password SET/VERIFY runtime enforcement;
- complete iOS/RCSP compatibility;
- automatic rollback after a successfully written but non-bootable OTA image;
- exact supplied GY-521 / MPU-6050 hardware qualification;
- MatrixPortal + external ICM-20689 hardware qualification;
- Waveshare on-board PCF85063, QMI8658, SHTC3, MicroSD and audio support;
- every possible logical/physical panel combination.

## Repository validation

The final stable tree must pass the dependency-free `tests/run_tests.py` suite from the extracted archive. Repository tests complement but do not replace real-device qualification.

## Toolchain note

The packaging environment used for Build 213 does not provide PlatformIO or Arduino CLI. A fresh all-environment PlatformIO compile is therefore not claimed from the packaging environment. The Waveshare 64x64 Build 212 predecessor was successfully compiled/uploaded and physically exercised before stable promotion; Build 213 changes only release/build identity and documentation/comments.
