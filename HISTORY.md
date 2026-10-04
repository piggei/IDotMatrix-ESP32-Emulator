# Release History

This file records public release milestones only. Internal development-build chronology is intentionally excluded from release packages; protocol evidence and qualification rationale live in the focused documentation under `docs/`.

## 0.6.0-rc.1

First release candidate for 0.6.0.

- Adds the Waveshare ESP32-S3 RGB Matrix as a hardware-qualified high-memory target with logical 16x16, 32x32 and 64x64 profiles on a physical 64x64 HUB75 panel.
- Adds dual-slot, physically triggered maintenance OTA on Waveshare. A short BOOT press performs a software reboot; holding BOOT for at least two seconds starts the temporary OTA access point.
- Adds guarded GIF PSRAM source staging, bounded compressed-source caching and one-item Carousel prefetch.
- Adds crash-recoverable transactional Carousel replacement and invocation-atomic Preset replacement.
- Adds static PNG Device Assets and hardware-qualified mixed PNG/GIF/TEXT Carousel playback.
- Hardens the Waveshare NimBLE host task for repeated Carousel replacement.
- Adds synthesized ES8311/I2S speaker notifications on Waveshare for BLE connection, Countdown, Program/Schedule and Alarm. No WAV/PCM samples are stored.
- Uses a hardware-qualified 100% default codec volume, configurable at compile time.
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
