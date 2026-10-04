# iDotMatrix ESP32 Emulator 0.6.0

**Release:** `0.6.0`  
**Build:** `213`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-B213`

Release 0.6.0 promotes the qualified 0.6.0 development line to stable. Build 213 changes release/build identity and public documentation only; runtime behavior is inherited from the hardware-qualified Build 212 state.

## Waveshare ESP32-S3 RGB Matrix

- Adds the ESP32-S3-N32R16 Waveshare target with 32 MB flash, 16 MB PSRAM and one physical 64x64 HUB75 panel.
- Provides logical 16x16, 32x32 and 64x64 profiles on the same physical panel; all three scaling paths were physically exercised.
- Uses the official/WLED-qualified HUB75 pin mapping and the established Waveshare Arduino/ESP-IDF platform baseline.
- Uses NimBLE-Arduino on Waveshare with an explicit 8 KiB host-task stack. Repeated real Carousel replacement was physically verified without recurrence of the earlier `nimble_host` stack-canary.

## OTA

- Adds physically triggered maintenance OTA through BOOT/GPIO0.
- Uses dual 3 MiB application slots and preserves the LittleFS media partition.
- Hardware testing confirmed successful OTA, interrupted-upload recovery to the previous firmware and persistence of stored state.
- Automatic post-boot rollback after a fully written but non-bootable image is not claimed by this release.

## Media and PSRAM

- Adds guarded whole-GIF compressed-source staging in PSRAM on Waveshare with LittleFS fallback.
- Adds a bounded persistent compressed-GIF PSRAM cache with LRU eviction and active-entry protection.
- Adds conservative one-item Carousel GIF prefetch.
- Keeps LittleFS authoritative and preserves a configurable PSRAM reserve.

## Transactional media hardening

- Moves final Preset/Carousel filesystem publication off the BLE host task and onto `loopTask`.
- Adds crash-recoverable whole-bank Carousel replacement using a persistent journal and rollback backups.
- Adds invocation-atomic volatile Preset/Default replacement with candidate staging and rollback/resume behavior.

## Device Assets / Carousel compatibility

- Supports static type-2 Device Assets entries, including the app-observed 64x64 8-bit RGBA PNG payload.
- Keeps exact logical RGB24 as an alternate static type-2 form.
- Supports mixed PNG/GIF/TEXT Carousel playback.
- Uses an 8-second upload-idle settle window because real official-app uploads were observed to pause for about 3.8 seconds between items without ending the bank transaction.
- Hardware testing confirmed TEXT remains under Carousel ownership and advances to the following item after its dwell.
- Repeated Carousel replacement was exercised on real Waveshare hardware without reboot or `nimble_host` stack-canary after the host-task stack hardening.

## Existing qualified functionality retained

The release retains the previously qualified BLE protocol, Clock, TEXT, image/GIF, Graffiti, Preset, Alarm, Program/Schedule, Countdown, Stopwatch, Scoreboard, Audio/Rhythm, DS3231 RTC, buzzer and orientation behavior from the 0.5.2 baseline and subsequent qualified development work.

## Explicit non-blocking exclusions

- password SET/VERIFY runtime support;
- complete iOS/RCSP compatibility;
- automatic post-boot OTA rollback;
- exact supplied GY-521 / MPU-6050 board qualification;
- MatrixPortal + external ICM-20689 qualification;
- Waveshare on-board PCF85063, QMI8658, SHTC3, MicroSD and audio support.

See `FUTURE-WORK.md` for the remaining research/extension list.
