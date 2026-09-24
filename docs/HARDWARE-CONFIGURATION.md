# Local Hardware Configuration

The optional local configuration header keeps board-specific wiring and peripheral policy outside tracked source files. It covers accelerometer selection and mounting, shared-I2C pins, RTC settings, buzzer hardware and sound-policy overrides.

## Create the local file

Copy the tracked template:

```bash
cp src/IDotMatrixUserConfig.example.h src/IDotMatrixUserConfig.h
```

The generated `src/IDotMatrixUserConfig.h` is intentionally excluded from Git and from release archives. `update_idotmatrix_emulator.sh` also preserves it while synchronizing a newer source package with `rsync --delete`.

If the file does not exist, the build behaves exactly as before and uses the selected PlatformIO profile defaults.

## Precedence

Configuration is resolved in this order:

1. explicit macros in `IDotMatrixUserConfig.h`;
2. `IDOTMATRIX_DEFAULT_*` values supplied by the PlatformIO environment;
3. driver/source defaults where applicable.

This means a local hardware definition can change the sensor address or mounting offset without editing `platformio.ini`. It can also select a different accelerometer backend when the required library dependencies are available in the chosen build profile.
When the local file explicitly changes only the backend, the previous profile-specific sensor address is not inherited; the newly selected backend falls back to its own safe address policy unless the local file also supplies `IDOTMATRIX_ACCEL_I2C_ADDRESS`.

## Available local settings

### Accelerometer backend

Select at most one:

```cpp
#define IDOTMATRIX_ACCEL_DRIVER_LIS3DH
// or
#define IDOTMATRIX_ACCEL_DRIVER_ICM20689
// or
#define IDOTMATRIX_ACCEL_DRIVER_MPU6050
```

Selecting more than one backend is rejected at compile time.

### I2C pins

Define both pins or neither:

```cpp
#define IDOTMATRIX_I2C_SDA_PIN 8
#define IDOTMATRIX_I2C_SCL_PIN 9
```

Defining only SDA or only SCL is rejected at compile time. When neither is supplied, a PlatformIO profile may provide `IDOTMATRIX_DEFAULT_I2C_SDA_PIN` / `IDOTMATRIX_DEFAULT_I2C_SCL_PIN`; otherwise the board's default `Wire` pins are used. The ESP32-C3 reference profile defaults to SDA GPIO1 and SCL GPIO2.

### Sensor address

```cpp
#define IDOTMATRIX_ACCEL_I2C_ADDRESS 0
```

For the ICM-20689 / MPU-family backend, `0` enables automatic probing of `0x68` followed by `0x69`. A fixed address can be used instead. The MatrixPortal LIS3DH profile defaults to `0x19`.

### Sensor mounting rotation

```cpp
#define IDOTMATRIX_ACCEL_MOUNT_ROTATION 90
```

Supported values are `0`, `90`, `180` and `270` degrees clockwise. The common orientation engine applies this offset after sensor-axis normalization.

## Example: external ICM-20689

```cpp
#pragma once

#define IDOTMATRIX_ACCEL_DRIVER_ICM20689
#define IDOTMATRIX_I2C_SDA_PIN 8
#define IDOTMATRIX_I2C_SCL_PIN 9
#define IDOTMATRIX_ACCEL_I2C_ADDRESS 0
#define IDOTMATRIX_ACCEL_MOUNT_ROTATION 0
```

This example keeps address auto-probing enabled. Change only the pins and mount rotation to match the actual wiring and physical installation.


## RTC configuration

The first persistent RTC backend is DS3231:

```cpp
#define IDOTMATRIX_RTC_TYPE IDOTMATRIX_RTC_DS3231
#define IDOTMATRIX_RTC_I2C_ADDRESS 0x68
#define IDOTMATRIX_RTC_SYNC_FROM_BLE 1
```

The DS3231 driver uses the shared `Wire` bus directly and never calls `Wire.begin()`. On the ESP32-C3 reference profile, that bus is GPIO1/GPIO2. If the RTC reports the oscillator-stop condition or an invalid date/time, it is not trusted until the app sends a valid time-sync command. With BLE synchronization enabled, the same command updates the hardware RTC.

For targeted bring-up, enable:

```cpp
#define IDOTMATRIX_RTC_DIAGNOSTICS 1
```

DS3231 uses fixed address `0x68`. An ICM-20689/MPU-family accelerometer on the same bus must therefore be strapped/configured at `0x69`. An explicit conflicting `0x68` accelerometer configuration is rejected at compile time.

## Buzzer configuration

Three compile-time buzzer backends are available:

```cpp
#define IDOTMATRIX_BUZZER_TYPE IDOTMATRIX_BUZZER_NONE
#define IDOTMATRIX_BUZZER_TYPE IDOTMATRIX_BUZZER_ACTIVE
#define IDOTMATRIX_BUZZER_TYPE IDOTMATRIX_BUZZER_PASSIVE
```

`ACTIVE` is intended for self-oscillating buzzer modules and drives a static GPIO level. `PASSIVE` uses the ESP32 LEDC peripheral to generate a 50% square-wave tone.

The reference ESP32-C3 profile defaults to:

```cpp
#define IDOTMATRIX_BUZZER_TYPE IDOTMATRIX_BUZZER_PASSIVE
#define IDOTMATRIX_BUZZER_PIN 3
#define IDOTMATRIX_BUZZER_FREQUENCY_HZ 2000
```

Override any of these locally if required. An explicit `IDOTMATRIX_BUZZER_NONE` disables the profile defaults without requiring the individual event flags to be cleared.

For active buzzer modules, polarity can be selected with:

```cpp
#define IDOTMATRIX_BUZZER_ACTIVE_HIGH 1
```

Individual local notification policies are independently controlled by:

```cpp
#define IDOTMATRIX_ALARM_BUZZER_ENABLED 1
#define IDOTMATRIX_COUNTDOWN_BUZZER_ENABLED 1
#define IDOTMATRIX_SCHEDULE_BUZZER_ENABLED 1
#define IDOTMATRIX_CONNECTION_BUZZER_ENABLED 1
```

The sound cadence is handled by the existing non-blocking state machine, so BLE processing and display refresh continue while the buzzer is active.

## Update-helper behavior

The repository update helper explicitly excludes `src/IDotMatrixUserConfig.h` from its destructive synchronization step. A local configuration therefore survives future source ZIP updates. The tracked `src/IDotMatrixUserConfig.example.h` may change as new hardware options are added; review it when adopting a newer build.


## Qualification diagnostics

Verbose sensor diagnostics are opt-in. To request continuous samples during a hardware qualification or troubleshooting run:

```cpp
#define IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS 1
```

To enable the detailed one-shot probe/configuration report:

```cpp
#define IDOTMATRIX_ORIENTATION_DIAGNOSTICS 1
```

For the ICM-20689 / MPU-family backend, the summary identifies probes at `0x68` and `0x69` separately and is repeated near the end of `setup()` for ESP32-C3 USB/CDC visibility. With verbose diagnostics disabled, a failed sensor initialization still produces one concise serial error.


## Passive buzzer trigger polarity

Three-wire passive buzzer modules may include a transistor driver and expose `VCC`, `GND` and `I/O`. The qualified ESP32-C3 reference module is marked **low level trigger** and is powered from 3.3 V with its `I/O` connected to GPIO3. The 5 V supply option advertised for some modules has not been qualified with a 3.3 V ESP32 control signal. Configure it with:

```cpp
#define IDOTMATRIX_BUZZER_TYPE IDOTMATRIX_BUZZER_PASSIVE
#define IDOTMATRIX_BUZZER_PIN 3
#define IDOTMATRIX_BUZZER_FREQUENCY_HZ 2000
#define IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW 1
```

With `IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW=1`, the firmware holds GPIO3 HIGH while silent. During a beep, LEDC generates the configured square wave. This prevents the module transistor from remaining enabled by a constant LOW level when no tone is requested. Direct passive buzzers normally use the default value `0`.
