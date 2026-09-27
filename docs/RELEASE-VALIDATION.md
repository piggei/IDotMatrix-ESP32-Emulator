# 0.5.2 Release Validation

**Release:** `0.5.2`  
**Build:** `190`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-B190`

This document records the qualification scope of the stable 0.5.2 release. Build 190 supersedes the initial stable Build 189 package with a tooling-only update: the repository updater now uses the checked-in dependency-free regression runner instead of requiring pytest, and the runner executes from the extracted archive root. Firmware runtime behavior is unchanged apart from the build identifier.

## Qualified reference targets

### Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75

Hardware-qualified baseline for the large-panel path, including representative BLE control, media/storage, Clock/TEXT, timers, automation, Audio/Rhythm and display-rotation behavior.

### ESP32-C3 + 16x16 WS2812

Hardware-qualified baseline for the compact path. The 0.5.2 line additionally qualifies the following peripherals and behaviors on ESP32-C3:

- passive three-wire low-level-trigger buzzer on GPIO3 at 2000 Hz, powered from 3.3 V, with HIGH idle level; the corrected idle behavior has been physically verified to remain cool and silent while inactive;
- DS3231 RTC on the shared GPIO1/GPIO2 I2C bus, including battery-backed retention, BLE time writeback, 60-second hot-recovery retry and cold boot into Clock when no persisted Carousel takes priority;
- persisted Carousel boot priority over the RTC Clock;
- no-Carousel/no-RTC boot fallback to screen off;
- Clock presentation persistence across power loss (style, 12/24-hour mode, date visibility and RGB color);
- Alarm operation after app disconnect and board reset;
- Program/Schedule operation after app disconnect and board reset;
- Countdown completion buzzer and BLE connection beep;
- external ICM-20689 automatic orientation on the tested shared-I2C configuration;
- post-hardening smoke tests for TEXT, Alarm, Carousel and Clock.

The DS3231 + gesture-sensor shared-bus configuration and the ICM-20689 + gesture-sensor shared-bus configuration were exercised separately. A simultaneous DS3231 + MPU-family accelerometer configuration requires the accelerometer at `0x69`; that three-device combination is not independently claimed as qualified.

## Functional regression scope

The release preserves the previously qualified protocol/runtime paths:

- BLE advertising, services, characteristics and Device Info;
- time synchronization, screen power, brightness and app-controlled flip;
- Clock, Countdown, Stopwatch and Scoreboard;
- TEXT static/multiline and scrolling modes;
- Solid, live Graffiti/DIY, full-raster Graffiti, RAW image and GIF;
- Device Assets Carousel and volatile Preset/Default;
- Alarm and Program/Schedule multipart media;
- LEVEL and FFT Audio/Rhythm framing and effects;
- LittleFS media ownership and persistence rules;
- normal Bulk `0x01..0x03` routing isolated from Graffiti full-raster `type=0x00`;
- automatic orientation at the final logical-to-physical output stage;
- boot-display priority: persisted Carousel first, otherwise valid RTC Clock, otherwise screen off.

## 0.5.2 hardening scope

The final 0.5.2 line includes the static-audit corrections completed before promotion:

- TEXT parser minimum-length guard rejects payloads that do not contain the first marker byte;
- software fallback timekeeping uses the 64-bit ESP timer monotonic clock instead of a 32-bit `millis()` epoch;
- `IDOTMATRIX_ACCEL_DRIVER_NONE` can explicitly suppress a profile-provided accelerometer backend;
- Adafruit LIS3DH is pinned to exact version `1.3.0`;
- the repository update helper runs the checked-in dependency-free Python regression runner before synchronization, requires explicit confirmation for a dirty Git tree and removes the selected PlatformIO environment build directory before build/upload;
- host-regression coverage exists for TEXT minimum-length handling, long-uptime timekeeping and passive-buzzer idle polarity;
- source comments and public documentation are maintained in English.

## Explicit exclusions

The following are outside the 0.5.2 release scope:

- complete iOS/RCSP compatibility on the diagnostic classic-ESP32 profile;
- password SET/VERIFY support;
- GY-521 / MPU-6050 hardware qualification (backend implemented; exact supplied board not yet physically qualified);
- MatrixPortal + external ICM-20689 hardware qualification;
- additional RTC models;
- qualification of every possible logical/physical scaling combination.

## Packaging checks

The final source package must contain no `.pio` output, Python cache, local `src/IDotMatrixUserConfig.h`, temporary diagnostic files or generated firmware binaries. Public documentation must be in English. Current release/build identifiers must resolve to `0.5.2 / Build 190`; historical identifiers may remain only in explicitly historical material.

All controller-board, LED-matrix and peripheral-module reference images present in the qualified documentation set must remain packaged and referenced.

## Build verification

Repository-level tests do not replace a real PlatformIO compile. The recommended clean source-to-binary verification commands are:

```bash
pio run -e matrixportal_s3_hub75_64
pio run -e matrixportal_s3_hub75_64_icm20689
pio run -e ios_compat_esp32_ws2812_32
pio run -e esp32c3_ws2812_16
```

The packaging environment used for Build 190 does not provide PlatformIO or Arduino CLI, so this document does not claim a fresh four-environment compile from that environment. The ESP32-C3 runtime paths changed during the 0.5.2 line were smoke-tested successfully on hardware before final promotion; MatrixPortal behavior is inherited from the previously qualified baseline for paths not changed afterward.
