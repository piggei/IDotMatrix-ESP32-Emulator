# Release History

This file records public release milestones only. Internal development-build chronology is intentionally excluded from release packages; protocol evidence and qualification rationale live in the focused documentation under `docs/`.

## 0.6.0

- Added the Waveshare ESP32-S3 RGB Matrix as a hardware-qualified high-memory target with logical 16x16, 32x32 and 64x64 profiles on a physical 64x64 HUB75 panel.
- Added dual-slot, physically triggered maintenance OTA on Waveshare. A short BOOT press performs a software reboot; holding BOOT for at least two seconds starts the temporary OTA access point.
- Added guarded GIF PSRAM source staging, bounded compressed-source caching and one-item Carousel prefetch.
- Added crash-recoverable transactional Carousel replacement and invocation-atomic Preset replacement.
- Added static Device Assets images using RGB24 or the app-observed PNG representation, with mixed PNG/GIF/TEXT Carousel playback.
- Moved persistent Waveshare Carousel receive buffering to PSRAM and filesystem publication to `loopTask`, eliminating LittleFS open/write work from the NimBLE receive callback for persistent GIF/IMAGE/TEXT assets.
- Retained an 8 KiB Waveshare NimBLE host-task stack as additional headroom.
- Added synthesized ES8311/I2S speaker notifications on Waveshare for BLE connection, Countdown, Program/Schedule and Alarm. No WAV/PCM samples are stored.
- Added BOOT short-press reboot / long-press OTA maintenance behavior.
- Hardware qualification includes repeated consecutive Carousel replacement without reboot, stack-canary or mixed-bank corruption.
- Device Assets bank replacement now establishes Carousel display ownership so a completed upload automatically switches from active Preset/Default playback to the newly committed Carousel.
- Retains the existing protocol, RTC, GPIO buzzer, orientation, display and media behavior on previously supported targets.

## 0.5.2

- Added DS3231 RTC support, persistent Clock presentation, passive-buzzer support and MPU-family orientation support.
- Improved boot policy and RTC recovery behavior.
- Hardened release/update tooling and dependency-free regression testing.

## 0.5.1

- Promoted automatic orientation and original-hardware Graffiti protocol reconstruction into the stable line.
- Preserved the existing BLE, display, media, Alarm and Schedule behavior.

## 0.5.0

- Established the standalone emulator baseline used by subsequent releases.
- Included the core BLE protocol, rendering, media, timers, Alarm, Program/Schedule and Audio/Rhythm visualization paths.

## 0.4.0

- Earlier public standalone-emulator milestone retained for historical continuity.
