# iDotMatrix ESP32 Emulator 0.5.2

**Release:** `0.5.2`  
**Build:** `190`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-B190`

Release 0.5.2 extends the 0.5.1 hardware baseline with RTC, passive-buzzer and MPU-family support, persistent Clock presentation, improved boot/recovery behavior and a final static-audit hardening pass. Build 190 is the current stable package. It supersedes Build 189 only to remove the updater's external pytest requirement; firmware runtime behavior is unchanged apart from the build identifier.

## Hardware and peripheral support

- Added an external ICM-20689 backend and shared MPU-6050-family driver support.
- Hardware-qualified ICM-20689 automatic orientation on ESP32-C3 with a shared I2C bus.
- Kept MPU-6050 implemented but unqualified until the supplied GY-521 candidate is physically identified and tested.
- Added optional local `src/IDotMatrixUserConfig.h` hardware overrides for accelerometer backend, I2C pins/address, mount rotation, RTC and buzzer settings.
- Added active/passive buzzer abstraction with non-blocking Alarm, Countdown, Program/Schedule and BLE connection patterns.
- Hardware-qualified the three-wire passive low-level-trigger buzzer module on ESP32-C3 GPIO3 at 2000 Hz; the module is held HIGH at idle and was confirmed to remain cool and silent.
- Added direct DS3231 RTC support on the shared I2C bus without third-party RTC initialization side effects.
- Hardware-qualified DS3231 battery retention, BLE writeback, cold-boot Clock fallback and hot recovery on ESP32-C3.

## Boot, Clock and autonomous operation

- Persisted Device Assets/Carousel has boot priority.
- With no usable Carousel, a valid RTC starts Clock automatically.
- With neither Carousel nor valid RTC, the display remains off.
- Clock style, 12/24-hour mode, date visibility and RGB text color persist in NVS across reboot/power loss.
- Alarm and Program/Schedule were verified after app disconnect and board reset.
- Countdown completion and BLE connection buzzer feedback were verified on the reference ESP32-C3 hardware.

## Parser and long-uptime hardening

- Fixed the TEXT parser minimum-length off-by-one so a header-only malformed payload is rejected before the first marker byte is read.
- Added boundary regression coverage for truncated TEXT payloads.
- Replaced the 32-bit `millis()` software-clock epoch with the 64-bit ESP timer monotonic timebase, eliminating the approximately 49.7-day fallback-clock rollover limitation.
- Added long-uptime regression coverage for the software timebase.

## Build and configuration hardening

- Added `IDOTMATRIX_ACCEL_DRIVER_NONE` to explicitly disable a profile-provided accelerometer backend.
- Pinned Adafruit LIS3DH to exact version `1.3.0`.
- Hardened the update helper: dependency-free checked-in regression tests run before synchronization, dirty Git state requires confirmation, the selected PlatformIO build directory is invalidated before build/upload, archive SHA-256 is shown when available and upload status is reported explicitly. Build 190 removes the previous requirement for pytest to be installed in the selected Python environment and runs the gate from the extracted archive root so repository-relative tests always inspect the selected package.
- Added host-testable passive-buzzer idle-polarity coverage.
- Cleaned remaining non-English source comments.

## Documentation and hardware catalog

- Consolidated current documentation around the stable 0.5.2 release while retaining development identifiers only in historical sections.
- Preserved the complete hardware image catalog for controller boards, LED matrices and documented peripheral modules.
- Added explicit qualification-state distinctions between tested hardware and implemented-but-unqualified hardware.

## Hardware qualification summary

The primary qualified configurations remain:

- Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75;
- ESP32-C3 + 16x16 WS2812.

The 0.5.2 ESP32-C3 qualification additionally covers the documented DS3231 RTC, low-level-trigger passive buzzer and external ICM-20689 configurations. The DS3231 + gesture and ICM-20689 + gesture shared-bus combinations were exercised separately; a simultaneous three-device DS3231 + MPU-family + gesture configuration is not independently claimed as qualified.

## Build-verification note

The release packaging environment does not provide PlatformIO or Arduino CLI, so no fresh four-environment PlatformIO build is claimed from that environment. Repository regression tests are executed from the final source archive with the standard-library-only `tests/run_tests.py` runner (pytest remains optional for developers), and the runtime paths changed during the 0.5.2 line were smoke-tested on the ESP32-C3 reference hardware.
