# iDotMatrix ESP32 Emulator 0.5.2-rc.1

**Release:** `0.5.2-rc.1`  
**Build:** `183`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-rc.1-B183`

This is the first release candidate for the 0.5.2 line. It promotes the hardware-tested Build 182 development baseline without adding new user-facing features.

## Highlights

### Expanded hardware compatibility

- Hardware-qualified external **ICM-20689** automatic-orientation backend on ESP32-C3 with a shared I2C bus.
- Shared MPU-family driver also contains an **MPU-6050** path; it remains implemented but not hardware-qualified.
- Optional `src/IDotMatrixUserConfig.h` keeps local sensor, I2C, RTC and buzzer wiring outside tracked files.

### Passive buzzer

- Hardware-qualified passive buzzer backend on ESP32-C3 GPIO3.
- Uses the ESP32 LEDC peripheral rather than a blocking software tone loop.
- Active buzzers remain supported.
- Alarm, Countdown, Program/Schedule and BLE connection notification policies remain independently configurable.

### DS3231 persistent RTC

- Hardware-qualified DS3231 backend on the ESP32-C3 shared GPIO1/GPIO2 I2C bus.
- Valid RTC time is available immediately after reboot and can start the device directly in Clock mode.
- The official-app time-sync command also updates the hardware RTC and clears the oscillator-stop condition.
- A configured but unavailable RTC is retried every 60 seconds by default.
- A valid RTC seeds the software clock so temporary later I2C failure does not immediately remove the time source.

### Clock presentation persistence

The following app-selected Clock settings now persist in NVS and are restored before the boot display policy:

- Clock style/layout;
- 12/24-hour mode;
- date visibility;
- RGB text color.

This allows an RTC-driven cold boot to reproduce the previous Clock presentation without opening the app.

### ESP32-C3 development quality

- Native USB CDC/JTAG serial defaults are encoded in the PlatformIO C3 profile.
- Deprecated manually-added `BLE2902` descriptors were removed; notification descriptors are provided automatically by the current BLE stack.
- RTC/orientation diagnostics remain available but are opt-in, with concise failure reporting retained for normal builds.

## Protocol compatibility

The RC does not intentionally change the already-qualified protocol paths. In particular:

- normal GIF/RAW/TEXT Bulk `0x01..0x03` remains unchanged;
- the dedicated original-hardware-confirmed full-raster Graffiti `type=0x00` path remains isolated from generic Bulk state;
- Audio/Rhythm LEVEL and FFT framing remains unchanged;
- Carousel, Preset, Alarm and Program/Schedule ownership/storage behavior remains unchanged.

## Qualification notes

The following 0.5.2 additions were physically exercised before the RC promotion:

- ESP32-C3 passive buzzer on GPIO3;
- ESP32-C3 ICM-20689 automatic orientation on the tested shared-I2C configuration;
- ESP32-C3 DS3231 with the gesture sensor sharing GPIO1/GPIO2, including battery retention, BLE time writeback and cold boot into Clock;
- Clock style/24-hour/color persistence across a full power cycle.

The ICM-20689 + gesture and DS3231 + gesture shared-bus configurations were validated separately. Simultaneous DS3231 + ICM-20689 requires the accelerometer to use `0x69` and is not separately claimed as qualified in RC1.

## Remaining non-blocking work

- MPU-6050 hardware qualification;
- complete password SET/VERIFY reverse engineering;
- isolated iOS/RCSP compatibility research;
- additional RTC/sensor backends and hardware combinations.

See `FUTURE-WORK.md` for the complete list.
