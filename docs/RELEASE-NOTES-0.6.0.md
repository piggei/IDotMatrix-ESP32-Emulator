# iDotMatrix ESP32 Emulator 0.6.0-rc.1

## Release candidate overview

`0.6.0-rc.1` is the first publication candidate for the 0.6.0 line. It consolidates the protocol, display, storage, maintenance and Waveshare audio work that has already passed physical hardware qualification. The RC does not add experimental features; its purpose is final release validation and packaging.

Public firmware signature:

```text
IDOTMATRIX_FW=0.6.0-rc.1
```

## Highlights

### Waveshare ESP32-S3 RGB Matrix

- ESP32-S3-N32R16, 32 MB flash and 16 MB PSRAM.
- One physical 64x64 HUB75 panel.
- Logical 16x16, 32x32 and 64x64 iDotMatrix profiles.
- Hardware-qualified scaling, BLE operation and persistent media behavior.
- 8 KiB NimBLE host-task stack on Waveshare profiles for robust repeated Carousel replacement.

### Media and Carousel

- Guarded whole-GIF source staging in PSRAM.
- Bounded compressed-source GIF cache and one-item Carousel prefetch.
- Transactional Carousel-bank replacement with boot recovery after interrupted updates.
- Invocation-atomic Preset/Default replacement.
- Static Device Assets images including the app-observed PNG representation.
- Mixed PNG/GIF/TEXT Carousel playback with correct return from TEXT to the following slot.
- Eight-second Device Assets settle window to tolerate observed pauses between items sent by the official app.

### Maintenance controls and OTA

On supported Waveshare profiles:

- short BOOT/GPIO0 press after normal startup: software reboot;
- BOOT hold for at least two seconds: temporary OTA maintenance access point;
- Wi-Fi remains disabled during normal operation;
- dual-slot OTA supports normal update and interrupted-upload recovery to the previous firmware.

### Synthesized Waveshare notification audio

The on-board ES8311 speaker path is supported without stored audio samples.

- ES8311 control: I2C1, address `0x18`, SDA47/SCL48.
- I2S1 TX: 48 kHz, 16-bit stereo.
- MCLK12, BCLK43, WS/LRCK38, DOUT21, PA GPIO11.
- 2 kHz square wave synthesized at runtime.
- Digital amplitude: 9000/32767.
- Default codec volume: 100%, compile-time configurable.
- Hardware-qualified notifications: BLE connection, Countdown completion, Program/Schedule and repeating Alarm.
- Existing non-blocking buzzer timing is reused: 90 ms pulse, 70 ms gap, three-pulse trill and 550 ms Alarm repeat pause.

### Existing functionality retained

The RC retains the previously qualified BLE protocol, Clock, TEXT, image/GIF, Graffiti, Preset, Alarm, Program/Schedule, Countdown, Stopwatch, Scoreboard, Audio/Rhythm visualization, DS3231 RTC, GPIO buzzer and orientation behavior.

## Deliberate exclusions

The following are not release blockers and are not claimed by 0.6.0-rc.1:

- password SET/VERIFY runtime support;
- complete iOS/RCSP compatibility;
- automatic rollback after a fully written but non-bootable OTA image;
- exact supplied GY-521 / MPU-6050 board qualification;
- MatrixPortal with external ICM-20689 qualification;
- Waveshare on-board PCF85063, QMI8658, SHTC3 and MicroSD support.

See `FUTURE-WORK.md` for the remaining research and extension list.
