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

## Current development target

### ESP32-S3

ESP32-S3 is the active v0.5 development target. The MatrixPortal S3 + native 64x64 HUB75 path is now the primary hardware-under-test.

Completed work includes native HUB75 64x64 output, large-media transfer validation, 64x64 TEXT/GIF/Carousel/Alarm/Schedule/Preset protocol testing, and direct comparison against an original 64x64 iDotMatrix. Remaining hardware work is focused on PSRAM policy, alternate logical/physical resolution combinations, optional RTC integration, and visual-quality follow-up.

## Support policy

For this project, a hardware target is considered **supported** only after end-to-end physical validation of the relevant display output, BLE protocol paths, storage, timers/events and persistence behavior.

A target that merely compiles or boots is considered experimental until that validation is complete.

## ESP32-S3 / MatrixPortal S3 / HUB75 (v0.5 development)

BUILD 123 introduces the first native HUB75 backend. Reference hardware:

- Adafruit MatrixPortal S3
- ESP32-S3, 8 MB flash, 2 MB PSRAM
- 64x64 RGB HUB75 panel, 1/32 scan
- ESP32-HUB75-MatrixPanel-DMA (WLED-native driver family)

The physical MatrixPortal/panel combination is now end-to-end validated as the active v0.5 development platform. Subsequent BUILD 125+ testing validated PlatformIO/LittleFS, BLE MTU 517, 64x64 TEXT/GIF/Carousel, large Alarm and Program/Schedule media, and the volatile Preset/Default bank on this target. Alternate logical/physical scaling combinations remain separate validation items.

Logical iDotMatrix profile and physical panel dimensions are independent. The output scaler is intended to support all 16x16, 32x32 and 64x64 source/destination combinations.

## PlatformIO reference environment (BUILD 125)

The MatrixPortal S3 development target now has a repository-controlled PlatformIO environment named `matrixportal_s3_hub75_64`. It pins Arduino-ESP32 3.3.11 through pioarduino and uses a project-local 8 MB partition table with a LittleFS-compatible data partition. This prevents the FAT-only Arduino IDE default from silently disabling GIF/media storage.

PlatformIO support does not change the hardware-support policy: a build environment is considered validated only after the firmware has been compiled, uploaded and exercised on the physical target. Arduino IDE remains an alternate supported build path during the transition.
