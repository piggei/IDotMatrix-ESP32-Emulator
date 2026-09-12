# Hardware Support Status

This document defines the hardware-support status for release `v0.4.0 / BUILD 118`.

## Supported

### Classic ESP32

The classic ESP32 is the reference and hardware-validated target for v0.4.0.

The validated development setup uses:

- classic ESP32 DollaTek/TTGO-style board;
- 16x16 WS2812B matrix;
- onboard SSD1306 OLED;
- active buzzer;
- LittleFS-backed media storage;
- official iDotMatrix app over BLE.

All v0.4.0 runtime behavior and release-candidate regression testing were performed against this target.

## Experimental / unsupported

### ESP32-C3

ESP32-C3 is **not officially supported in v0.4.0**.

Direct testing showed that, with the classic-board OLED disabled and pins remapped to C3-safe GPIOs, the firmware can:

- boot successfully;
- allocate the logical frame buffers;
- mount LittleFS;
- load Alarm/Schedule state;
- restore brightness state;
- continue well into normal initialization.

However, current FastLED output produced repeated channel/driver timeout messages during matrix refresh. The exact FastLED/C3 backend issue was not pursued because C3 support is not required for the v0.4.0 release.

The classic ESP32 default pin mapping must also not be copied directly to ESP32-C3. Board-specific GPIO restrictions differ.

Therefore:

- compilation alone does not imply support;
- successful boot alone does not imply support;
- no ESP32-C3 compatibility guarantee is provided for v0.4.0.

## Planned

### ESP32-S3

ESP32-S3 is the planned next major hardware target.

Expected work includes:

- native HUB75 64x64 output;
- PSRAM-aware buffering;
- larger-media stress testing;
- native 64x64 TEXT/GIF validation;
- optional RTC integration;
- comparison against the physical original 64x64 iDotMatrix.

## Support policy

For this project, a hardware target is considered **supported** only after end-to-end physical validation of the relevant display output, BLE protocol paths, storage, timers/events and persistence behavior.

A target that merely compiles or boots is considered experimental until that validation is complete.
