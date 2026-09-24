# Local Hardware Configuration

Build 174 adds an optional local configuration header for board-specific sensor wiring and orientation settings.

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

Defining only SDA or only SCL is rejected at compile time. When neither is supplied, the board's default `Wire` pins are used.

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

## Update-helper behavior

The repository update helper explicitly excludes `src/IDotMatrixUserConfig.h` from its destructive synchronization step. A local configuration therefore survives future source ZIP updates. The tracked `src/IDotMatrixUserConfig.example.h` may change as new hardware options are added; review it when adopting a newer build.


## Qualification diagnostics

Build 176 returns verbose sensor diagnostics to opt-in operation after successful ICM-20689 qualification. To request continuous samples during a hardware qualification or troubleshooting run:

```cpp
#define IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS 1
```

To enable the detailed one-shot probe/configuration report:

```cpp
#define IDOTMATRIX_ORIENTATION_DIAGNOSTICS 1
```

For the ICM-20689 / MPU-family backend, the summary identifies probes at `0x68` and `0x69` separately and is repeated near the end of `setup()` for ESP32-C3 USB/CDC visibility. With verbose diagnostics disabled, a failed sensor initialization still produces one concise serial error.
