# iDotMatrix ESP32 Emulator 0.6.0-dev.2

**Release:** `0.6.0-dev.2`  
**Build:** `192`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.2-B192`

## Purpose

Build 192 is a corrective Waveshare bring-up build. Build 191 failed its first physical hardware gate: the HUB75 panel remained blank and the native USB serial device repeatedly disconnected/reconnected, indicating a reset-loop class failure before the platform could be qualified.

## Corrective changes

- The Waveshare environment now uses the same Tasmota Arduino Core 3.3.8 / ESP-IDF 5.5.4 platform baseline as the already-qualified WLED Waveshare target.
- The 32 MB partition geometry now matches the WLED Waveshare baseline: two 3 MiB OTA slots, filesystem beginning at `0x610000`, and a final 64 KiB coredump partition.
- The profile no longer forces `board_build.flash_mode=opi`. The `esp32s3camlcd` board definition and `memory_type=opi_opi` provide the required OPI boot configuration while flash access remains DOUT.
- Serial checkpoints were added immediately before and after `MatrixPanel_I2S_DMA::begin()` so a remaining reset can be localized to the HUB75 initialization path.

## Deliberately unchanged

The HUB75 GPIO map remains the WLED-qualified Waveshare mapping. OTA remains physically triggered through BOOT/GPIO0. GIF PSRAM staging, source cache, prefetch, MicroSD, PCF85063, QMI8658, SHTC3 and audio remain deferred.

## Field gate

The first Build 192 test is intentionally narrow:

1. compile the `waveshare_s3_rgbmatrix_64x64` environment;
2. USB flash the board;
3. verify that native USB serial remains continuously connected;
4. confirm the `entering HUB75 initialization` and `returned from HUB75 initialization` checkpoints;
5. verify native 64x64 panel output;
6. only after stable boot, continue with BLE and OTA qualification.
