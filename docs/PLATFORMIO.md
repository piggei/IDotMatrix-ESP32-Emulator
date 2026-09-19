# PlatformIO Build Guide

PlatformIO is the recommended reproducible build path for the standalone emulator. Arduino IDE compatibility is retained where practical, but PlatformIO encodes the reference toolchain, libraries, partitions and target-specific build flags in source control.

## Checked-in environments

The repository currently provides:

```text
matrixportal_s3_hub75_64
ios_compat_esp32_ws2812_32
esp32c3_ws2812_16
```

### `matrixportal_s3_hub75_64`

- board: Adafruit MatrixPortal ESP32-S3;
- logical 64x64 profile;
- physical 64x64 HUB75;
- project-local 8 MB partition table;
- LittleFS;
- pinned HUB75 DMA driver;
- primary hardware-qualified large-panel target.

### `esp32c3_ws2812_16`

- board: `esp32-c3-devkitm-1`;
- logical/physical 16x16;
- WS2812 on GPIO4;
- LittleFS with `min_spiffs.csv`;
- hardware validated.

This environment overrides `lib_deps` so PlatformIO does not build `ESP32-HUB75-MatrixPanel-DMA` on the C3 target. That avoids pulling in HUB75/Adafruit_GFX dependencies for a WS2812-only build.

### `ios_compat_esp32_ws2812_32`

- classic ESP32;
- logical 32x32 profile;
- physical 16x16 WS2812 on GPIO17;
- iOS handshake experiment enabled.

This is a diagnostic environment and is not part of the main release qualification.

The checked-in `default_envs` value may point at a diagnostic target. For reproducible work, always pass `-e <environment>` explicitly.

## Reference toolchain

The qualified baseline is:

```text
Arduino-ESP32 3.3.11
ESP-IDF       5.5.5
```

The project therefore uses pioarduino:

```text
https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
```

Primary libraries:

```text
FastLED                         3.10.3
AnimatedGIF                     2.2.3
ESP32-HUB75-MatrixPanel-DMA     pinned commit for HUB75 target
```

BLE, Preferences and LittleFS come from the pinned Arduino-ESP32 framework.

## Build

From the repository root:

```bash
pio run -e matrixportal_s3_hub75_64
```

ESP32-C3:

```bash
pio run -e esp32c3_ws2812_16
```

iOS diagnostic target:

```bash
pio run -e ios_compat_esp32_ws2812_32
```

The first build can take longer because PlatformIO downloads the platform and dependencies. Incremental builds reuse the local package/cache state.

## Upload

MatrixPortal S3:

```bash
pio run -e matrixportal_s3_hub75_64 -t upload
```

ESP32-C3:

```bash
pio run -e esp32c3_ws2812_16 -t upload
```

Do not commit machine-specific serial ports to `platformio.ini`.

## MatrixPortal S3 USB behavior

The MatrixPortal S3 exposes different persistent USB identities during programming and normal runtime:

```text
Programming / JTAG:
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00

Runtime monitor:
/dev/serial/by-id/usb-Adafruit_MatrixPortal_ESP32-S3_*-if00
```

The JTAG identity appears only after the upload process has already initiated the programming transition. Therefore the repository update helper intentionally runs the PlatformIO upload target directly instead of waiting for JTAG before starting `pio`.

After upload, the helper waits for the Adafruit runtime endpoint before opening the monitor.

## Serial monitor

```bash
pio device monitor -e matrixportal_s3_hub75_64
```

The reference speed is 115200 baud. Under Linux/WSL, prefer persistent `/dev/serial/by-id` names over `/dev/ttyACM*`.

`MONITOR_SERIAL_PORT` can be used by the repository helper to override automatic runtime-port discovery.

## MatrixPortal S3 partition layout

The emulator stores received media in Arduino LittleFS. The MatrixPortal target uses:

```text
partitions/idotmatrix_matrixportal_s3_8mb.csv
```

The 8 MB layout provides:

| Region | Offset | Size | Purpose |
| --- | ---: | ---: | --- |
| NVS | `0x009000` | 20 KB | Preferences/NVS |
| factory app | `0x010000` | 4 MB | Firmware |
| TinyUF2 reserved | `0x410000` | 1 MB | Adafruit board tooling |
| `spiffs` data | `0x510000` | 2.9375 MB | Mounted by Arduino LittleFS |

The partition name/subtype remains `spiffs` for compatibility with Arduino `LittleFS.begin()`. `board_build.filesystem = littlefs` controls PlatformIO filesystem tooling; it does not redefine the partition subtype used by the Arduino core.

The reference layout intentionally uses one large factory application partition and no OTA application slot. Do not silently shrink the application partition to add OTA without a separately validated memory/partition design.

Normal firmware updates should not upload an empty filesystem image unless intentionally erasing runtime media.

## Build-time display configuration

The source contains Arduino-IDE-compatible defaults, while PlatformIO environments override target selection through build flags:

```text
DISPLAY_BACKEND
IDOTMATRIX_SCREEN_TYPE
PHYSICAL_MATRIX_WIDTH
PHYSICAL_MATRIX_HEIGHT
MATRIX_PIN
```

Logical and physical resolutions are independent. The final renderer/output stage handles direct copy, nearest-neighbor upscaling and box-average downscaling.

## HUB75 backend

The MatrixPortal environment uses `ESP32-HUB75-MatrixPanel-DMA` with the project pin mapping, 8-bit color depth, single DMA buffering and the S3-specific flags carried by the qualified configuration.

Brightness is applied through the HUB75 output-enable/PWM path (`setBrightness8`) instead of destructively scaling the framebuffer RGB values.

## Arduino IDE compatibility

Arduino IDE builds use the defaults in `src/IDotMatrix.ino`. For the MatrixPortal S3 reference hardware, manually reproduce the critical PlatformIO choices:

- Arduino-ESP32 3.3.x compatible with the qualified baseline;
- a partition layout with a LittleFS-compatible data partition;
- the pinned HUB75 DMA driver family;
- compatible graphics dependencies when required by the selected driver version.

PlatformIO remains preferred because those choices are encoded in the repository rather than selected manually from IDE menus.

## Optional accelerometer backends

Orientation support is selected by the sensor backend, not by the ESP32 family.

The MatrixPortal S3 environment currently enables:

```ini
-DIDOTMATRIX_ACCEL_DRIVER_LIS3DH
-DIDOTMATRIX_ACCEL_I2C_ADDRESS=0x19
```

This automatically enables the common `IDOTMATRIX_ORIENTATION_SENSOR` subsystem.

An ESP32-C3 or classic ESP32 can use the same feature in the future by attaching a supported external sensor and enabling its `IDOTMATRIX_ACCEL_DRIVER_*` backend. Targets without a selected backend do not compile the orientation code or sensor library.
