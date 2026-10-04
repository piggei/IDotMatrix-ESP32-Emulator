# iDotMatrix ESP32 Emulator 0.6.0

## Release overview

`0.6.0` is the stable release of the Waveshare expansion and media-hardening cycle. It preserves the established standalone iDotMatrix protocol implementation while adding a high-memory Waveshare ESP32-S3 RGB Matrix target, maintenance OTA, PSRAM-backed media optimizations, transactional media replacement, static PNG Device Assets and synthesized on-board speaker notifications.

Public firmware signature:

```text
IDOTMATRIX_FW=0.6.0
```

## Highlights

### Waveshare ESP32-S3 RGB Matrix

- ESP32-S3-N32R16 with 32 MB flash and 16 MB PSRAM.
- One physical 64x64 HUB75 panel.
- Logical 16x16, 32x32 and 64x64 iDotMatrix profiles.
- Hardware-qualified scaling, BLE operation and persistent media behavior.
- 8 KiB NimBLE host-task stack on Waveshare profiles.
- Persistent Carousel GIF/IMAGE/TEXT assets are buffered in PSRAM; LittleFS open/write/publication runs on `loopTask`.
- The legacy persistent Carousel GIF/TEXT filesystem-receive path is excluded from Waveshare builds.

### Media and Carousel

- Guarded whole-GIF source staging in PSRAM.
- Bounded compressed-source GIF cache and one-item Carousel prefetch.
- Transactional Carousel-bank replacement with boot recovery after interrupted updates.
- Invocation-atomic Preset/Default replacement.
- Static Device Assets images using exact logical RGB24 or the app-observed PNG representation.
- Mixed PNG/GIF/TEXT Carousel playback, including correct transition from TEXT to the following slot.
- Eight-second Device Assets settle window to tolerate observed pauses between items sent by the official app.
- Hardware qualification includes three consecutive complete Carousel replacements without reboot or `nimble_host` stack-canary, with playback resuming after each commit.
- Device Assets bank setup establishes Carousel view intent, so a completed upload switches from active Preset/Default playback to the first valid slot of the newly committed Carousel even if the app does not send another Assets-view command after the transfer.

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
- Existing non-blocking notification timing is reused: 90 ms pulse, 70 ms gap, three-pulse trill and 550 ms Alarm repeat pause.

### Existing functionality retained

The release retains Clock, TEXT, image/GIF, Graffiti, Preset, Alarm, Program/Schedule, Countdown, Stopwatch, Scoreboard, Audio/Rhythm visualization, DS3231 RTC, GPIO buzzer and orientation behavior from the previously qualified standalone line.

## Important engineering fixes included in 0.6.0

During qualification, filesystem activity from the BLE/NimBLE receive callback was found to be unsafe under repeated persistent media replacement. The final design keeps persistent Waveshare Carousel receive memory-only on `nimble_host`: complete GIF/IMAGE/TEXT assets are accumulated in guarded PSRAM buffers and filesystem publication is deferred to `loopTask`. This is an ownership-boundary fix, not merely a larger task stack.

The official app was also observed to send a static Carousel entry as Bulk type `0x02` containing a 64x64 RGBA PNG rather than a one-frame GIF. The final parser treats type `0x02` as a static-image container and accepts both exact RGB24 and PNG content.

A separate display-ownership issue was found during final release validation: a bank could commit successfully while an active Preset/Default continued to own the display. The final state machine treats the Device Assets bank-setup command as persistent Carousel view intent and preserves any later explicit Assets-view request, so commit/settle reliably transfers ownership to the new Carousel.

## Deliberate exclusions

The following are outside the 0.6.0 release scope:

- password SET/VERIFY runtime support;
- complete iOS/RCSP compatibility;
- automatic rollback after a fully written but non-bootable OTA image;
- exact supplied GY-521 / MPU-6050 board qualification;
- MatrixPortal with external ICM-20689 qualification;
- Waveshare on-board PCF85063, QMI8658, SHTC3 and MicroSD support.

See `FUTURE-WORK.md` for the remaining research and extension list.
