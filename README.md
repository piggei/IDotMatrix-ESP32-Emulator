# iDotMatrix ESP32 Emulator

ESP32 firmware that emulates an iDotMatrix BLE display and communicates directly with the official iDotMatrix app. The project reproduces the observed device protocol on common ESP32 hardware, renders the app's content locally, and documents the reverse-engineered behavior of the original displays.

<p align="center">
  <img src="assets/idotmatrix-esp32-demo.jpg" alt="iDotMatrix ESP32 Emulator" width="700">
</p>

The emulator is based on official-app BLE captures, differential testing and direct comparison with original iDotMatrix hardware.

## Release

- **Release:** `0.5.2-rc.2`
- **Build:** `184`

The firmware embeds the signature:

```text
IDOTMATRIX_FW=0.5.2-rc.2-B184
```

The public release number identifies the software version. The build number identifies the exact internal source state used to produce the firmware.

Release candidate notes: [`docs/RELEASE-NOTES-0.5.2-rc.2.md`](docs/RELEASE-NOTES-0.5.2-rc.2.md).

The latest stable public release remains `0.5.1 / Build 172`.

## What the emulator supports

The current implementation includes:

- BLE advertising, services and characteristics compatible with the official app;
- app-facing device information and logical 16x16, 32x32 and 64x64 profiles;
- time synchronization, screen power, brightness, power saving and app-controlled 180-degree rotation;
- Clock, Countdown, Stopwatch and Scoreboard;
- TEXT with 16/32/64-pixel glyph families, scrolling, paging and multiline non-scrolling layouts;
- Solid, live Graffiti/DIY, full-raster Graffiti, image and GIF content;
- the persistent 12-slot Device Assets Carousel;
- the volatile six-slot Preset/Default bank (`14..19`) with mixed TEXT/GIF playback;
- Alarm and Program/Schedule, including validated multipart media handling;
- Audio/Rhythm with five LEVEL and five FFT effects;
- LittleFS-backed media storage and transactional replacement where required;
- DS3231 RTC support with persistent timekeeping, BLE time synchronization and shared-I2C operation;
- independent logical and physical display resolutions with nearest-neighbor upscaling and box-average downscaling;
- hardware-qualified automatic orientation with the MatrixPortal S3 LIS3DH and an external ICM-20689 validated on ESP32-C3 with a shared I2C bus;
- generic orientation mount compensation and optional compile-time I2C pin overrides for external sensors;
- active and passive buzzer backends with non-blocking Alarm, Countdown, Schedule and connection notification patterns.

Protocol details, confidence levels and original-device observations are documented in [`PROTOCOL.md`](PROTOCOL.md).

## Supported hardware

### Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75

This is the primary large-panel reference platform and is hardware validated end to end.

Reference configuration:

- Adafruit MatrixPortal ESP32-S3;
- ESP32-S3, 8 MB flash, 2 MB PSRAM;
- 64x64 RGB HUB75 panel, 1/32 scan;
- `ESP32-HUB75-MatrixPanel-DMA` output backend;
- LittleFS media storage;
- BLE MTU 517.

### ESP32-C3 + 16x16 WS2812

The native 16x16 ESP32-C3 profile is also hardware validated. The ICM-20689 orientation backend has additionally been validated on ESP32-C3 with the I2C bus shared with the gesture sensor, and the passive buzzer on GPIO3 is hardware-qualified. The DS3231 RTC backend is hardware-qualified on the same shared I2C bus, using GPIO1 as SDA and GPIO2 as SCL in the reference profile. Battery-backed retention, BLE time writeback and cold boot into Clock when no persisted Carousel takes priority have been validated on the ESP32-C3 target. The checked-in PlatformIO environment uses a WS2812-only dependency set and does not build the HUB75 driver on this target; external sensor, RTC and buzzer settings can be overridden through the optional local hardware configuration.

### Classic ESP32 + WS2812

Classic ESP32 remains supported by the source architecture. A dedicated environment is retained for isolated iOS compatibility research.

See [`docs/HARDWARE-SUPPORT.md`](docs/HARDWARE-SUPPORT.md) for target-specific details.

## Display architecture

The logical iDotMatrix profile exposed to the app is independent from the physical panel attached to the ESP32.

Supported logical sizes:

- 16x16 (`screenType=0x01`);
- 32x32 (`screenType=0x03`);
- 64x64 (`screenType=0x04`).

Supported physical output families:

- WS2812/FastLED;
- HUB75 DMA on ESP32-S3.

When logical and physical dimensions differ, the final output stage performs nearest-neighbor upscaling, box-average downscaling, or direct copy for matching dimensions. This keeps protocol behavior and renderer geometry independent from the actual panel resolution.

## Persistent and volatile media

The emulator intentionally keeps Device Assets and Preset/Default as two distinct systems.

**Device Assets / Carousel** is a persistent 12-slot bank. GIF and TEXT items are stored in LittleFS and can be restored by the boot policy.

**Preset / Default** is a volatile six-slot bank using device slots `14..19`. Assets are staged separately from Carousel content and become active only after the `06/02` activation command. Preset media is not restored after reboot/reset.

Live content takes display ownership immediately. Entering ordinary TEXT, GIF, Clock or another display mode stops active Preset playback and cancels active/pending Carousel playback without deleting their stored assets.

## Audio / Rhythm

The official app sends already-derived audio visualization data over BLE; the ESP32 does not need to capture microphone audio.

Two wire-frame families are implemented:

- LEVEL: fixed 6-byte frames, five effects based on one global level value;
- FFT: fixed 21-byte frames containing 16 wire bands and five effects.

The FFT transport is treated as a byte stream because one BLE ATT write may contain a complete frame plus part of the next one. The 16 transmitted bands are reduced to eight logical renderer bands by averaging adjacent pairs.

LEVEL and FFT rendering and transport have been hardware validated on the MatrixPortal S3 reference target.

## Graffiti full-raster transport

The emulator includes the hardware-captured 64x64 Graffiti full-raster transport used by the official Android app. This path is separate from normal 16-byte GIF/RAW/TEXT Bulk: each logical Graffiti packet has a 9-byte header, a first/continuation marker (`0x00` / `0x02`), the complete raster size, and up to 4096 RGB bytes. On original hardware the device replies `05 00 00 00 02` after incomplete chunks and `05 00 00 00 01` after the complete raster. The emulator publishes the framebuffer only after all `width * height * 3` bytes have arrived.

The raw HCI-derived exchange is summarized in [`docs/captures/14-graffiti-original-hardware.txt`](docs/captures/14-graffiti-original-hardware.txt).

## Automatic orientation support

The orientation subsystem is compile-time gated. Automatic display rotation is hardware-qualified on MatrixPortal S3 using its on-board LIS3DH and on ESP32-C3 using an external ICM-20689 on a shared I2C bus. MPU-6050 support shares the same low-level family driver but remains implemented and unqualified. The validated normalized mapping is `+Y`=0 deg, `+X`=90 deg, `-Y`=180 deg and `-X`=270 deg. External or custom-mounted sensors can compensate their planar mounting orientation at compile time with `IDOTMATRIX_ACCEL_MOUNT_ROTATION=0|90|180|270`.

Rotation is applied only in the final logical-to-physical output mapping, so the qualified TEXT, media, Clock, timer, scoreboard, Audio/Rhythm and automation renderers remain unchanged. Sensor-specific backends automatically enable the common orientation engine; targets without an `IDOTMATRIX_ACCEL_DRIVER_*` selection compile without accelerometer code or dependencies. See [`docs/ORIENTATION-SENSOR.md`](docs/ORIENTATION-SENSOR.md).

## RTC support

The DS3231 backend provides battery-backed persistent time on the shared I2C bus. The first implemented device is the DS3231 at I2C address `0x68`. The driver talks directly to the already initialized `Wire` instance and never reinitializes the bus, which is important on the ESP32-C3 reference hardware where the RTC shares GPIO1/GPIO2 with other I2C peripherals.

At boot, a valid DS3231 becomes the authoritative clock source and seeds the software clock as a fallback for temporary later I2C failures. If the oscillator-stop flag is set or the stored date/time is invalid, the RTC is ignored until the official app sends its normal time-synchronization command. With `IDOTMATRIX_RTC_SYNC_FROM_BLE=1` (the default), every valid app time sync also updates the DS3231 and clears the oscillator-stop condition. Alarm, Program/Schedule, ECO timing and Clock rendering can therefore continue across MCU reboots without requiring a new BLE connection.

Boot display priority is deterministic: a valid persisted Device Assets/Carousel is resumed first; if no stored Carousel can start, a valid RTC starts Clock; otherwise the display remains off. This preserves persistent Carousel intent while still allowing standalone RTC-backed Clock operation when no Carousel is configured.

Clock presentation settings are stored separately from the RTC in NVS. The latest app-selected Clock style, 12/24-hour mode, date visibility and RGB text colour are restored before the boot display policy runs, so an RTC-driven Clock fallback renders the same Clock appearance that was active before power loss. Writes are deferred and coalesced to avoid unnecessary flash wear when the app repeats the same Clock command.

The DS3231 address is fixed at `0x68`. If an ICM-20689/MPU-family accelerometer shares the same I2C bus, it must be physically configured at `0x69`; the firmware rejects an explicit `0x68` accelerometer configuration when DS3231 support is enabled. Auto-probe remains supported and tries `0x69` first in that configuration. If the configured RTC is unavailable, the firmware retries detection every 60 seconds by default; `IDOTMATRIX_RTC_RETRY_INTERVAL_MS` can override that interval. A BLE time-sync received while the RTC is offline schedules an immediate loop-side reprobe, and a recovered invalid RTC is updated from the current synchronized software time.

## Build with PlatformIO

The repository contains four explicit environments:

```text
matrixportal_s3_hub75_64
matrixportal_s3_hub75_64_icm20689
ios_compat_esp32_ws2812_32
esp32c3_ws2812_16
```

The reference toolchain is Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5 through pioarduino.

The ESP32-C3 PlatformIO profile enables native USB CDC/JTAG (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`) so firmware `Serial` diagnostics are visible on the same native USB connection used for development.

Build the MatrixPortal target with:

```bash
pio run -e matrixportal_s3_hub75_64
```

Upload with:

```bash
pio run -e matrixportal_s3_hub75_64 -t upload
```

Build the MatrixPortal external ICM-20689 profile with:

```bash
pio run -e matrixportal_s3_hub75_64_icm20689
```

### Arduino IDE note for ESP32-C3

When using the ESP32-C3 native USB connector in Arduino IDE, set **USB CDC On Boot = Enabled** and **USB Mode = Hardware CDC and JTAG**. PlatformIO profile defaults are not available to Arduino IDE builds, so board-specific RTC, buzzer and I2C settings must be present explicitly in `src/IDotMatrixUserConfig.h` when compiling outside PlatformIO.

### Optional local hardware configuration

Board-specific sensor wiring and peripheral overrides can be kept in an optional local include. Copy the tracked template:

```bash
cp src/IDotMatrixUserConfig.example.h src/IDotMatrixUserConfig.h
```

Then uncomment only the settings required by the local hardware. The header can select the accelerometer backend, define I2C pins, sensor address and mount compensation, configure RTC behavior/retry timing, and override the buzzer backend/pin/frequency without editing `platformio.ini` or tracked source files. For example:

```cpp
#pragma once
#define IDOTMATRIX_ACCEL_DRIVER_ICM20689
#define IDOTMATRIX_I2C_SDA_PIN 8
#define IDOTMATRIX_I2C_SCL_PIN 9
#define IDOTMATRIX_ACCEL_I2C_ADDRESS 0
#define IDOTMATRIX_ACCEL_MOUNT_ROTATION 90
// Optional: continuous XYZ logging during qualification.
// #define IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS 1
```

Verbose orientation diagnostics are disabled by default. Set `IDOTMATRIX_ORIENTATION_DIAGNOSTICS=1` for the detailed probe/configuration summary; when enabled it is repeated near the end of `setup()` so ESP32-C3 USB serial sessions that attach late can still see it. A normal build still emits one concise error if sensor initialization fails.

`src/IDotMatrixUserConfig.h` is ignored by Git and preserved by `update_idotmatrix_emulator.sh` when a new source archive is synchronized. PlatformIO sensor settings are now profile defaults, so explicit values in the local header take precedence.

The C3 profile defaults to `IDOTMATRIX_BUZZER_PASSIVE` on GPIO3 at 2000 Hz. Active buzzer modules remain supported, and any individual buzzer event can be disabled locally.

See [`docs/HARDWARE-CONFIGURATION.md`](docs/HARDWARE-CONFIGURATION.md) for precedence and examples.

Build the ESP32-C3 target with:

```bash
pio run -e esp32c3_ws2812_16
```

See [`docs/PLATFORMIO.md`](docs/PLATFORMIO.md) for the complete toolchain, partition and USB workflow.

## MatrixPortal S3 reference pinout

```text
R1=42  G1=41  B1=40
R2=38  G2=39  B2=37
A=45   B=36   C=48   D=35   E=21
LAT=47 OE=14  CLK=2
```

The project-local 8 MB partition table is stored in `partitions/idotmatrix_matrixportal_s3_8mb.csv`.

## Original-device policy

Some emulator behaviors intentionally differ from the original 64x64 device. Examples include preserving transient Cloud/Graffiti content after leaving the app section, persistent DS3231 RTC support, a one-shot Program notification, and a Countdown completion buzzer.

These differences are documented explicitly rather than presented as protocol facts. See [`docs/ORIGINAL-HARDWARE-64X64.md`](docs/ORIGINAL-HARDWARE-64X64.md) and [`docs/PROTOCOL-COMPARISON.md`](docs/PROTOCOL-COMPARISON.md).

## Repository update helper

[`update_idotmatrix_emulator.sh`](update_idotmatrix_emulator.sh) synchronizes a source archive into a local Git checkout, invokes the selected PlatformIO upload target, and waits for the normal Adafruit runtime serial endpoint before opening the monitor.

The helper deliberately does not wait for the Espressif JTAG endpoint before starting PlatformIO upload, because that USB identity appears only after the programming transition has already begun.

## Security considerations

The emulated compatibility BLE profile follows the behavior required by the official app and is not a secure authenticated control channel. CRC32 is used for media integrity, not authentication.

Password SET/VERIFY behavior remains only partially reverse engineered and is not implemented as a supported runtime feature.

Do not expose the device in environments where unauthenticated BLE control would be unacceptable.

## Documentation

- [`PROTOCOL.md`](PROTOCOL.md) — protocol reference and confidence levels
- [`HISTORY.md`](HISTORY.md) — public release history
- [`FUTURE-WORK.md`](FUTURE-WORK.md) — non-blocking research and possible extensions
- [`docs/HARDWARE-SUPPORT.md`](docs/HARDWARE-SUPPORT.md) — supported hardware and qualification policy
- [`docs/HARDWARE-CONFIGURATION.md`](docs/HARDWARE-CONFIGURATION.md) — local sensor/I2C override file and precedence
- [`docs/ORIENTATION-SENSOR.md`](docs/ORIENTATION-SENSOR.md) — accelerometer driver abstraction and orientation diagnostics
- [`docs/PLATFORMIO.md`](docs/PLATFORMIO.md) — reproducible PlatformIO build/upload guide
- [`docs/ORIGINAL-HARDWARE-64X64.md`](docs/ORIGINAL-HARDWARE-64X64.md) — direct observations from original hardware
- [`docs/PROTOCOL-COMPARISON.md`](docs/PROTOCOL-COMPARISON.md) — comparison with independent implementations
- [`docs/RELEASE-VALIDATION.md`](docs/RELEASE-VALIDATION.md) — final release validation scope
- [`docs/RELEASE-NOTES-0.5.2-rc.2.md`](docs/RELEASE-NOTES-0.5.2-rc.2.md) — 0.5.2 RC2 feature and qualification summary
- [`docs/RELEASE-AUDIT-0.5.2-rc.2.md`](docs/RELEASE-AUDIT-0.5.2-rc.2.md) — RC2 source/documentation/package audit
- [`docs/RELEASE-AUDIT-0.5.1.md`](docs/RELEASE-AUDIT-0.5.1.md) — historical 0.5.1 release audit

## Related project

The WLED integration is maintained separately as **IDotMatrix WLED Usermod**. The standalone emulator and the WLED Usermod share protocol findings where useful, but remain independent implementations.

## License

See [`LICENSE`](LICENSE).
