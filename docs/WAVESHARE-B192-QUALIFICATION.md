# Waveshare ESP32-S3 RGB Matrix - Build 192 Qualification

**Release:** `0.6.0-dev.2`  
**Build:** `191`  
**Target:** Waveshare ESP32-S3 RGB Matrix / ESP32-S3-N32R16  
**Status:** development bring-up; field qualification required

## Purpose

Build 192 is the first standalone-emulator target for the Waveshare ESP32-S3 RGB Matrix. It deliberately limits the change set to board/display bring-up, 32 MB flash/16 MB PSRAM configuration, and an OTA maintenance foundation.

The following on-board devices are intentionally **not** enabled in this build:

- PCF85063 RTC;
- QMI8658 IMU;
- SHTC3 temperature/humidity sensor;
- MicroSD storage;
- ES8311/ES7210 audio path.

They will be introduced independently after the board/display/OTA baseline is qualified.

## Hardware profile

- MCU: ESP32-S3-N32R16;
- flash: 32 MB octal;
- PSRAM: 16 MB octal;
- physical display: one 64x64 HUB75 panel;
- logical iDotMatrix profile: 64x64 (`screenType=0x04`);
- HUB75 library: `ESP32-HUB75-MatrixPanel-DMA`, pinned to the same commit used by the existing standalone MatrixPortal target;
- OTA trigger: BOOT / GPIO0, active-low, 2 s hold while firmware is already running.

## HUB75 pinout

```text
R1=4   G1=5   B1=6
R2=7   G2=15  B2=16
A=18   B=8    C=3    D=42   E=9
LAT=40 OE=2   CLK=41
```

This mapping matches the Waveshare example/WLED `WAVESHARE_S3_PINOUT` configuration used by the WLED iDotMatrix 0.9.4 qualification line.

## Static package gates

Before field testing, the source package must pass:

- dependency-free repository regression suite;
- Waveshare environment/pinout checks;
- OTA partition-layout checks;
- OTA inactive-slot/update-path checks;
- preprocessor balance;
- package hygiene.

A full PlatformIO firmware compile cannot be claimed unless it is executed in an environment with the pioarduino toolchain available.

## Field test sequence

### Gate A - boot and memory

Serial diagnostics should report:

- `IDOTMATRIX_FW=0.6.0-dev.2-B192`;
- MCU ESP32-S3;
- board Waveshare ESP32-S3 RGB Matrix;
- flash approximately 32 MB;
- PSRAM approximately 16 MB;
- OTA enabled/armed.

### Gate B - HUB75

Verify:

- clean boot;
- correct red/green/blue channel order;
- no mirrored/scrambled rows;
- correct 64x64 geometry;
- Clock;
- TEXT;
- static image;
- GIF;
- Carousel;
- Graffiti/full-raster content.

### Gate C - BLE

Verify:

- advertising;
- app connection;
- disconnect/reconnect;
- media transfer;
- commands continue to work while OTA is not active.

### Gate D - OTA

1. Hold BOOT for about 2 s after startup.
2. Join the `IDotMatrix-OTA-XXXXXX` access point.
3. Open `http://192.168.4.1/`.
4. Upload a valid Waveshare `firmware.bin` with a distinguishable build number.
5. Confirm reboot into the new build.
6. Confirm persistent data and LittleFS media remain available.

### Gate E - interrupted OTA

Begin another upload and interrupt the transfer before completion. Reboot/power-cycle if needed and confirm that the previously valid firmware remains bootable.

## Exit criteria

Build 192 is qualified when HUB75/BLE behavior is equivalent to the existing MatrixPortal standalone baseline and the OTA happy-path plus interrupted-upload path pass on the physical Waveshare board.

Only after this gate should the 0.6.0 line proceed to scaling qualification and PSRAM measurement/optimization work.
## Build 191 field result

Build 191 failed the first physical Waveshare gate: the panel remained blank and the native USB serial device repeatedly disconnected/reconnected, consistent with a reset loop. Build 192 supersedes B191 for all further field testing. The corrective build removes two unnecessary differences from the working WLED Waveshare baseline: it uses the same Tasmota Arduino 3.3.8 / ESP-IDF 5.5.4 platform and the same 32 MB OTA/filesystem geometry. It also adds serial checkpoints immediately before and after HUB75 initialization.

