# 0.6.0-dev.2 / Build 192 Development Audit

**Release:** `0.6.0-dev.2`  
**Build:** `192`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.2-B192`

## Scope

Build 192 is a targeted correction of the failed Build 191 Waveshare bring-up. It does not add feature scope.

## Verified statically

- Waveshare board remains `esp32s3camlcd` with `memory_type=opi_opi`, 32 MB flash and PSRAM enabled.
- HUB75 GPIO mapping is unchanged from the WLED-qualified Waveshare mapping.
- Waveshare framework is pinned to the same Tasmota Arduino 3.3.8 / ESP-IDF 5.5.4 baseline used by the working WLED target.
- OTA/filesystem partition offsets match the WLED 32 MB geometry and fill exactly 32 MB without overlap.
- No future peripheral backend was enabled accidentally.
- OTA remains outside the runtime-state mutex and continues to use the standard Arduino `Update` API.
- Serial checkpoints bracket HUB75 initialization.

## Regression suite

The dependency-free repository regression suite passes all 43 tests from the Build 192 source tree.

## Open hardware gate

A complete PlatformIO build and physical Waveshare qualification must be performed on the target system. Build 192 is not considered hardware-qualified until the reset loop is eliminated and the HUB75/USB/BLE/OTA field gates pass.
