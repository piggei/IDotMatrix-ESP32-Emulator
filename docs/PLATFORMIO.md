# PlatformIO build guide

## Status

PlatformIO support was introduced in `v0.5.0-dev / BUILD 125`.

The recommended **main-branch** development target is currently:

- Adafruit MatrixPortal S3;
- ESP32-S3;
- 8 MB flash;
- 2 MB PSRAM;
- 64x64 HUB75 physical panel;
- iDotMatrix logical profile `0x04` (64x64).

Arduino IDE remains supported. PlatformIO is recommended for iterative development because it provides reproducible toolchain/library versions, incremental builds and project-local partition configuration.

## Why pioarduino is used

The project reference toolchain is Arduino-ESP32 `3.3.11` / ESP-IDF `5.5.5`.

The `platformio.ini` therefore uses the pioarduino ESP32 platform release matching Arduino-ESP32 3.3.11 instead of the older official PlatformIO ESP32 Arduino package.

The platform is pinned to:

```text
https://github.com/pioarduino/platform-espressif32/releases/download/55.03.311/platform-espressif32.zip
```

Do not replace this with Arduino-ESP32 `4.0.0-alpha` for the reference build. The project pins the Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5 PlatformIO toolchain used for current MatrixPortal development.

## First build

Open the repository root in VS Code with PlatformIO/pioarduino installed, or use the command line from the repository root:

```text
pio run
```

On the main branch / BUILD 141 consolidation package, the default environment is `matrixportal_s3_hub75_64`. In this BUILD 142 `ios-dev` package, the default is intentionally `ios_dev_esp32_ws2812_32` so Thiago cannot accidentally build the MatrixPortal target.

The main MatrixPortal target is still available explicitly:

```text
pio run -e matrixportal_s3_hub75_64
```

The first build is expected to take longer because PlatformIO must download the platform, framework and libraries. Subsequent incremental builds should reuse those packages and only rebuild changed translation units/dependencies.

## BUILD 142 iOS development target

The `ios-dev` branch adds a second environment for Thiago's hardware and makes it the default environment in the BUILD 142 package:

```text
ios_dev_esp32_ws2812_32
```

Configuration:

```text
board                = esp32dev
display backend      = WS2812 / FastLED
logical profile      = 32x32 / screen type 0x03
physical matrix      = 16x16
WS2812 GPIO          = 17
partition layout     = min_spiffs.csv
IOS_HANDSHAKE_EXPERIMENT = 1
```

Build explicitly with:

```text
pio run -e ios_dev_esp32_ws2812_32
```

Upload with:

```text
pio run -e ios_dev_esp32_ws2812_32 -t upload
```

The root `update_idotmatrix_emulator.sh` remains tailored to PJ's MatrixPortal S3 / WSL USB workflow and should not be used as Thiago's generic classic-ESP32 uploader.

## Upload

Connect the MatrixPortal S3 over USB and run:

```text
pio run -e matrixportal_s3_hub75_64 -t upload
```

If more than one serial device is present, set a local `upload_port` in `platformio.ini` or pass the port from the command line. Do not commit a machine-specific COM port to the repository.

## Serial monitor

```text
pio device monitor -b 115200
```

or:

```text
pio device monitor -e matrixportal_s3_hub75_64
```

For MatrixPortal S3 development under Linux/WSL, prefer the persistent `/dev/serial/by-id` entry rather than `/dev/ttyACM*`. The TinyUSB by-id name may include a board-specific MAC suffix after firmware changes. The repository helper therefore auto-discovers:

```text
/dev/serial/by-id/usb-Adafruit_MatrixPortal_ESP32-S3*-if00
```

Set `SERIAL_PORT=/dev/serial/by-id/...` when an explicit override is needed. The helper performs serial discovery before compilation, compiles without uploading, verifies the generated firmware signature, then performs a **second mandatory serial check before upload**. This is intentional for WSL/usbip workflows where the MatrixPortal may move between runtime/data and programming/JTAG USB endpoints. After upload, the helper also requires the normal detach/reattach cycle before opening the runtime monitor.

## LittleFS and partition layout

The emulator stores received GIF/media data through Arduino LittleFS. The MatrixPortal S3 Arduino IDE defaults can expose a FAT-oriented layout, which leaves the emulator without the `spiffs`-subtype partition that Arduino LittleFS expects by default.

BUILD 125 removes that build-time ambiguity by selecting:

```text
partitions/idotmatrix_matrixportal_s3_8mb.csv
```

The 8 MB layout is:

| Region | Offset | Size | Purpose |
| --- | ---: | ---: | --- |
| NVS | `0x009000` | 20 KB | Preferences/NVS |
| factory app | `0x010000` | 4 MB | Firmware |
| TinyUF2 reserved | `0x410000` | 1 MB | Reserved for Adafruit board tooling |
| `spiffs` data | `0x510000` | 2.9375 MB | Mounted by Arduino LittleFS |

The partition is named `spiffs` for compatibility with the default Arduino `LittleFS.begin()` partition label. `board_build.filesystem = littlefs` controls PlatformIO filesystem tooling; it does not change the partition subtype required by Arduino LittleFS.

The current table intentionally provides one large factory application partition and no OTA application slot. `OTA_ENABLED` is disabled by default in the firmware. If OTA becomes a supported requirement, introduce a separately validated OTA-capable partition table rather than shrinking the application partition silently.

Do **not** upload an empty filesystem image during normal development: received GIFs and persistent media are runtime data. Firmware upload and filesystem upload are separate operations.

## Pinned libraries

The current reference environment retains the primary dependency versions introduced during the BUILD 125 PlatformIO migration:

```text
FastLED                  3.10.3
AnimatedGIF              2.2.3
ESP32-HUB75-MatrixPanel-DMA  WLED-pinned 3.0.14 commit
```

Built-in Arduino-ESP32 libraries such as BLE, Preferences and LittleFS come from the pinned framework.

## Build-time display configuration

The source still contains Arduino-IDE-compatible defaults, but BUILD 125 makes the main target selections overrideable from PlatformIO `build_flags`:

```text
DISPLAY_BACKEND
IDOTMATRIX_SCREEN_TYPE
PHYSICAL_MATRIX_WIDTH
PHYSICAL_MATRIX_HEIGHT
MATRIX_PIN
```

The current PlatformIO environment explicitly selects:

```text
DISPLAY_BACKEND_HUB75
logical 64x64 / screen type 0x04
physical 64x64
```

Future 16x16/32x32 logical and physical test environments can therefore be added without editing the renderer or scaler. The logical-to-physical scaler remains responsible for all supported upscaling, downscaling and 1:1 combinations.

## Arduino IDE compatibility

Arduino IDE builds continue to use the defaults in `src/IDotMatrix.ino`.

For the current MatrixPortal S3 reference setup, Arduino IDE must still use:

- Arduino-ESP32 stable 3.3.x;
- a partition scheme containing a SPIFFS/LittleFS-compatible data partition;
- ESP32-HUB75-MatrixPanel-DMA at the WLED-pinned 3.0.14 commit;
- Adafruit GFX 1.12.6.

PlatformIO is recommended because these project-critical choices are encoded in source control rather than selected manually from IDE menus.

## BUILD 130 HUB75 backend

The MatrixPortal S3 environment uses the same HUB75 driver family as WLED: `ESP32-HUB75-MatrixPanel-DMA`. The dependency is pinned to the same upstream commit used by WLED at the time BUILD 130 was prepared. The 64x64 MatrixPortal path uses the exact board pinout, 8-bit color depth, single DMA buffering, `clkphase=false`, `NO_CIE1931`, and the S3 LCD divider flag used by WLED.

Brightness is applied through the HUB75 driver's output-enable timing (`setBrightness8`) rather than by reducing framebuffer RGB values. This preserves gradient resolution at low panel brightness.
