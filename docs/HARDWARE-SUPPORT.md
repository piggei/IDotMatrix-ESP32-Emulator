# Hardware Support Status

This document describes the hardware targets and current qualification status for the v0.5.x emulator line.

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
- hardware-qualified three-wire passive low-level-trigger buzzer module on GPIO3 at 2000 Hz;
- hardware-qualified DS3231 RTC on the shared GPIO1/GPIO2 I2C bus, including battery-backed retention, BLE writeback and cold-boot Clock fallback when no persisted Carousel starts;
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

### Buzzer backends

The firmware supports both self-oscillating active buzzers and passive buzzers. Passive output uses the ESP32 LEDC hardware peripheral and therefore does not depend on timing loops in the main firmware. Passive trigger polarity is configurable with `IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW`. The reference ESP32-C3 profile selects the qualified three-wire transistor module on GPIO3 at 2000 Hz with low-level triggering and 3.3 V module supply; while silent, GPIO3 is held HIGH so the module transistor is off and the buzzer is not DC-biased. Buzzer backend, GPIO, frequency, trigger polarity and per-event policies can be overridden in `IDotMatrixUserConfig.h`.

## Orientation sensor support

The MatrixPortal S3 LIS3DH backend is enabled on the `matrixportal_s3_hub75_64` profile and remains hardware-qualified. The common orientation layer supports a compile-time planar mounting offset (`IDOTMATRIX_ACCEL_MOUNT_ROTATION=0|90|180|270`).

The external ICM-20689 backend is selected with `IDOTMATRIX_ACCEL_DRIVER_ICM20689`. It identifies the sensor with `WHO_AM_I=0x98`, uses the common orientation engine, and is **hardware-qualified on ESP32-C3 with the I2C bus shared with the gesture sensor**. The same low-level driver also contains an MPU-6050 path for `WHO_AM_I=0x68/0x69`; that MPU-6050 path remains implemented but unqualified.

A dedicated `matrixportal_s3_hub75_64_icm20689` PlatformIO environment is provided for an external ICM-20689 on MatrixPortal. It auto-probes I2C addresses `0x68` and `0x69`; that specific board/sensor combination has not been separately hardware-qualified. External boards can override the I2C pins with `IDOTMATRIX_I2C_SDA_PIN` and `IDOTMATRIX_I2C_SCL_PIN`. Verbose probe and XYZ diagnostics are opt-in.

The feature remains backend-driven rather than board-driven. Targets without a selected accelerometer backend do not compile or link the orientation subsystem.

## Local sensor wiring configuration

`src/IDotMatrixUserConfig.h` is available as an optional, untracked local hardware layer. Use it for external-sensor backend selection, SDA/SCL pins, sensor address and mount rotation. See [`HARDWARE-CONFIGURATION.md`](HARDWARE-CONFIGURATION.md).

## DS3231 RTC

The direct DS3231 backend provides hardware-qualified persistent timekeeping. The ESP32-C3 reference profile enables it at the fixed DS3231 address `0x68` on the shared I2C bus:

- SDA: GPIO1;
- SCL: GPIO2;
- address: `0x68`;
- BLE time synchronization: enabled.

The RTC driver uses the existing `Wire` object and does not initialize or reconfigure the bus. A valid RTC is used immediately after boot. If the DS3231 oscillator-stop flag is set, the stored time is treated as invalid until the app sends a valid time-sync packet. If the RTC is unavailable, the firmware retries detection every 60 seconds by default and can synchronize a recovered invalid RTC from the current software clock.

Because the DS3231 address is fixed at `0x68`, an ICM-20689/MPU-family device sharing the same bus must use `0x69`. The firmware rejects an explicit accelerometer `0x68` selection when the DS3231 backend is enabled.

The DS3231 integration is hardware-qualified on ESP32-C3 with the gesture sensor sharing GPIO1/GPIO2. The ICM-20689 shared-bus orientation path was qualified separately on the same controller family. Simultaneous DS3231 + ICM-20689 operation requires the accelerometer to be physically strapped to `0x69` and is not claimed as a separately qualified three-device combination. Additional RTC models remain future work.
