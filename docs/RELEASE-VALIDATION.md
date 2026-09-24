# 0.5.2-rc.2 Release Validation

**Release:** `0.5.2-rc.2`  
**Build:** `184`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-rc.2-B184`

This document defines the qualification scope for the second 0.5.2 release candidate. It supersedes the historical 0.5.0 validation document for current release work.

## Qualified reference targets

### Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75

Hardware-qualified baseline for the large-panel path, including representative BLE control, media/storage, Clock/TEXT, timers, automation, Audio/Rhythm and display-rotation behavior.

### ESP32-C3 + 16x16 WS2812

Hardware-qualified baseline for the compact path. The 0.5.2 line additionally qualifies the following peripherals on ESP32-C3:

- passive buzzer on GPIO3 using the LEDC backend;
- DS3231 RTC on the shared GPIO1/GPIO2 I2C bus, including battery-backed retention, BLE time writeback and cold boot into Clock when no persisted Carousel starts;
- Clock presentation persistence across power loss (style, 12/24-hour mode, date visibility and RGB color);
- external ICM-20689 automatic orientation on the tested shared-I2C configuration.

The DS3231 + gesture-sensor shared-bus configuration and the ICM-20689 + gesture-sensor shared-bus configuration were exercised separately. A simultaneous DS3231 + MPU-family accelerometer configuration requires the accelerometer at `0x69`; that three-device combination is not independently claimed as qualified by RC2.

## Functional regression scope

The RC must preserve the previously qualified protocol/runtime paths:

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
- automatic orientation at the final logical-to-physical output stage.
- boot-display priority: persisted Carousel first, otherwise valid RTC Clock, otherwise screen off.

## 0.5.2 RC additions

The 0.5.2 RC line adds or consolidates:

- ICM-20689 orientation backend and shared MPU-family driver;
- optional `IDotMatrixUserConfig.h` local hardware layer;
- passive and active buzzer backends;
- DS3231 persistent RTC backend using the already-initialized shared `Wire` bus;
- RTC retry/recovery every 60 seconds by default when configured but unavailable;
- software-clock seeding from a valid RTC for temporary I2C-failure fallback;
- Clock presentation persistence in NVS;
- native USB CDC/JTAG serial configuration on the ESP32-C3 PlatformIO profile;
- removal of manually-added deprecated BLE2902 descriptors.

## Explicit exclusions

The following do not block RC2:

- complete iOS/RCSP compatibility on the diagnostic classic-ESP32 profile;
- password SET/VERIFY support;
- MPU-6050 hardware qualification;
- MatrixPortal + external ICM-20689 hardware qualification;
- additional RTC models;
- qualification of every possible logical/physical scaling combination.

## Packaging checks

The RC package must contain no `.pio` output, Python cache, local `src/IDotMatrixUserConfig.h`, temporary diagnostic files or generated firmware binaries. Public documentation must be in English. Current release/build identifiers must resolve to `0.5.2-rc.2 / Build 184`; historical identifiers may remain only where explicitly describing older builds/releases.

## RC2 publication gate

Static checks and source/documentation audit do not replace a final on-device smoke test of Build 184. Build 184 intentionally changes only boot-display priority from the RC1 baseline: persisted Carousel must win over RTC-backed Clock. Any other unexpected runtime difference should block publication and be treated as a regression.
