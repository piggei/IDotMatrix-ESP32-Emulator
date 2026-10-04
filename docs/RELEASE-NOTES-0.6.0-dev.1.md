# iDotMatrix ESP32 Emulator 0.6.0-dev.1

**Release:** `0.6.0-dev.1`  
**Build:** `191`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.1-B191`

## Purpose

Build 191 starts the 0.6.0 development line from the stable `0.5.2 / Build 190` baseline. Its scope is deliberately narrow: first standalone bring-up of the Waveshare ESP32-S3 RGB Matrix and an OTA maintenance foundation.

## Added

- `waveshare_s3_rgbmatrix_64x64` PlatformIO environment;
- ESP32-S3-N32R16 memory profile: 32 MB flash / 16 MB PSRAM;
- official/WLED-qualified Waveshare HUB75 GPIO mapping;
- dedicated 32 MB dual-slot OTA partition table;
- approximately 19.94 MiB LittleFS data partition after the two application slots;
- physically triggered OTA maintenance AP and HTTP firmware upload page;
- OTA configuration overrides in `IDotMatrixUserConfig.example.h`;
- startup diagnostics for board, flash, PSRAM and OTA state;
- host regression coverage for the Waveshare profile and OTA layout/path.

## OTA policy

Normal runtime keeps Wi-Fi disabled. Holding BOOT/GPIO0 for approximately two seconds after firmware startup starts the temporary OTA access point. Firmware is written to the inactive OTA application slot with Arduino-ESP32 `Update` and committed only after the complete upload finalizes successfully.

Build 191 does not yet claim automatic rollback from an image that uploads successfully but later fails at startup.

## Deliberately deferred

Build 191 does not yet enable the Waveshare on-board PCF85063 RTC, QMI8658 IMU, SHTC3 sensor, MicroSD or audio hardware. GIF PSRAM staging, persistent source cache and Carousel prefetch are also deferred until the base board/OTA path is physically qualified.

## Qualification status

Static regression tests pass in the packaging environment. A full pioarduino build and physical Waveshare test are required before this development build can be considered qualified.

See `docs/WAVESHARE-B191-QUALIFICATION.md` for the field gate and `docs/OTA.md` for the update workflow.
