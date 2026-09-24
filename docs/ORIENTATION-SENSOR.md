# Orientation Sensor Architecture

## Status

The common orientation engine is hardware-qualified with the MatrixPortal S3 onboard LIS3DH and with an external ICM-20689 on the tested ESP32-C3 shared-I2C configuration. MPU-6050 support is implemented through the same MPU-family backend but remains unqualified. Rotation stays isolated at the final framebuffer-to-output mapping stage.

Validated MatrixPortal S3 mapping:

| Physical panel orientation | Stable gravity direction | Display orientation |
| --- | --- | --- |
| Upright / 0 deg | `+Y` | `0 deg` |
| 90 deg clockwise | `+X` | `90 deg` |
| 180 deg | `-Y` | `180 deg` |
| 270 deg clockwise | `-X` | `270 deg` |

## Design goals

The accelerometer backend is independent from the common display-orientation logic.

A target selects a sensor backend, for example:

```ini
-DIDOTMATRIX_ACCEL_DRIVER_LIS3DH
```

Any supported `IDOTMATRIX_ACCEL_DRIVER_*` backend automatically enables:

```text
IDOTMATRIX_ORIENTATION_SENSOR=1
```

Targets without a selected accelerometer backend do not compile the orientation code and do not pull sensor-specific libraries.

This allows the same feature to be used with integrated sensors or with external accelerometers attached to boards such as ESP32-C3 or classic ESP32.

## Architecture

```text
accelerometer driver
        |
        | normalized X/Y/Z acceleration in g
        v
IDotMatrixAccelerometer
        |
        v
IDotMatrixOrientation
        |
        | dominant gravity direction
        | hysteresis
        | stability timer
        v
stable display rotation
        |
        v
final logical-to-physical output mapping
```

The sensor-specific driver is responsible for:

- sensor initialization;
- raw sensor acquisition;
- conversion to normalized acceleration in `g`;
- any future sensor-specific axis normalization required to match the common panel coordinate convention.

The common orientation layer is responsible for:

- sampling cadence;
- dominant X/Y gravity classification;
- diagonal hysteresis;
- candidate tracking;
- stable-orientation timing;
- gravity-direction to display-rotation mapping;
- final output-coordinate rotation.

The common layer contains no LIS3DH register or library logic.

## MatrixPortal S3 / LIS3DH

The MatrixPortal S3 profile enables:

```ini
-DIDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH
-DIDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS=0x19
-DIDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION=0
-DIDOTMATRIX_ORIENTATION_AUTO_ROTATE=1
```

The LIS3DH backend currently uses:

- +/-2 g range;
- 50 Hz sensor data rate;
- 100 ms firmware sampling interval;
- 600 ms candidate stability time;
- 0.55 g minimum dominant horizontal-axis threshold;
- 0.12 g X/Y diagonal hysteresis.

Hardware testing of Build 163 confirmed the complete clockwise sequence:

```text
+Y -> +X -> -Y -> -X -> +Y
```

with stable transitions and no observed orientation chatter.

## Automatic rotation behavior

Rotation is applied at the final output stage rather than inside individual renderers.

Therefore the same transform automatically applies to:

- TEXT;
- static images;
- GIF playback;
- Carousel;
- Preset/Default;
- Clock;
- Countdown;
- Stopwatch;
- Scoreboard;
- Audio/Rhythm;
- procedural effects;
- Alarm and Program/Schedule content.

The qualified logical framebuffer remains unchanged. Only the final mapping from the rendered image to the physical output is transformed.

For a 90-degree physical clockwise rotation, the displayed image is compensated by the inverse transform so that the content remains upright to the viewer.

The existing app-controlled 180-degree flip remains a separate output transform and is preserved.

## Stability and flat-panel behavior

A new candidate orientation must remain stable for approximately 600 ms before it is accepted.

Near the 45-degree diagonal, the current candidate/stable axis is retained while it remains plausible. This avoids repeated changes between X and Y.

When neither X nor Y has sufficient gravity magnitude, for example while the panel is close to flat, the classifier returns `UNKNOWN` and the last stable display orientation is preserved.

## Diagnostics

Verbose orientation diagnostics are opt-in. When enabled, initialization and actual rotation changes are reported, for example:

```text
ORIENTATION SENSOR: driver=LIS3DH init=OK
ORIENTATION AUTO-ROTATE: enabled
ORIENT ROTATION -> 90
ORIENT ROTATION -> 180
```

Continuous X/Y/Z sample logging is separately controlled by:

```text
IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS
```

and is disabled by default.

## Compile-time controls

`IDOTMATRIX_ORIENTATION_AUTO_ROTATE=0` can be used to keep sensor acquisition and orientation classification available without applying the final display rotation.

This is useful for future sensor qualification and board bring-up.

## External ICM-20689 backend

The external ICM-20689 backend is selected with:

```text
IDOTMATRIX_ACCEL_DRIVER_ICM20689
```

The backend originated from the separately tested WLED implementation and is **hardware-qualified in the emulator on ESP32-C3 with the I2C bus shared with the gesture sensor**.

Identification and configuration:

- I2C address: `0x68` or `0x69`;
- `WHO_AM_I=0x98` for ICM-20689;
- 50 Hz output configuration;
- +/-2 g accelerometer range;
- accelerometer data starts at register `0x3B`;
- ICM-20689 accelerometer DLPF uses `ACCEL_CONFIG2` at `0x1D`;
- critical configuration registers are read back after initialization.

Set `IDOTMATRIX_ACCEL_I2C_ADDRESS=0` to auto-probe `0x68` then `0x69`. A fixed address can be supplied instead.

A MatrixPortal external-sensor PlatformIO profile is also provided:

```text
matrixportal_s3_hub75_64_icm20689
```

This profile selects the ICM-20689 backend and auto-probes `0x68/0x69`. The specific MatrixPortal + external-ICM combination has not been separately qualified. Enable `IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS=1` locally when normalized X/Y/Z values are needed for bring-up.

For boards whose external I2C wiring does not use the framework defaults, define both:

```text
IDOTMATRIX_I2C_SDA_PIN=<gpio>
IDOTMATRIX_I2C_SCL_PIN=<gpio>
```

## MPU-6050 compatibility path

The same low-level register driver also implements an MPU-6050 path selected with:

```text
IDOTMATRIX_ACCEL_DRIVER_MPU6050
```

It accepts `WHO_AM_I=0x68` or `0x69` and intentionally omits the ICM-20689-only `ACCEL_CONFIG2` configuration. This code path is implemented but has not yet been hardware-qualified in the emulator project.
## Sensor mounting compensation

The accelerometer driver and the common orientation engine are intentionally independent from the mechanical mounting of the sensor PCB.

Use:

```ini
-DIDOTMATRIX_ACCEL_MOUNT_ROTATION=0
```

Supported values are `0`, `90`, `180`, and `270`, expressed as the clockwise planar rotation of the accelerometer PCB relative to the display. Invalid values fail compilation.

Examples:

```ini
; Sensor aligned with the display
-DIDOTMATRIX_ACCEL_MOUNT_ROTATION=0

; Same sensor mounted 90 degrees clockwise on a custom board
-DIDOTMATRIX_ACCEL_MOUNT_ROTATION=90

; External sensor module mounted upside down in the display plane
-DIDOTMATRIX_ACCEL_MOUNT_ROTATION=180
```

The common orientation engine subtracts this mounting offset from the orientation detected from normalized X/Y data. This keeps renderer and display-rotation logic independent from a particular PCB layout.

This option handles planar rotations. A future sensor backend whose raw chip axes differ from the common normalized convention should normalize those axes inside the backend before returning `IDotMatrixAccelSample`.


## Local hardware include

Create `src/IDotMatrixUserConfig.h` from `src/IDotMatrixUserConfig.example.h` to keep board-specific sensor choices and wiring local. It may define the backend, SDA/SCL pins, sensor I2C address and `IDOTMATRIX_ACCEL_MOUNT_ROTATION`.

The local file takes precedence over `IDOTMATRIX_DEFAULT_*` values supplied by PlatformIO profiles. It is ignored by Git and preserved by the repository update helper. See [`HARDWARE-CONFIGURATION.md`](HARDWARE-CONFIGURATION.md).


## Qualification diagnostics

The detailed probe/configuration report used during ESP32-C3 qualification is retained but disabled by default. Set `IDOTMATRIX_ORIENTATION_DIAGNOSTICS=1` to report independent `0x68`/`0x69` probes, ACK state, `WHO_AM_I`, backend match, active address and configuration result.

When enabled, the diagnostic summary is repeated near the end of `setup()` because native USB/CDC serial on ESP32-C3 can attach after the first sensor probe has completed. Continuous XYZ output remains independently controlled by `IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS`.
