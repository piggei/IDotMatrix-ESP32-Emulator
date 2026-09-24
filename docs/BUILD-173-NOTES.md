# Build 173 Development Notes

**Release:** `0.5.2-dev`  
**Build:** `173`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-dev-B173`

## Purpose

Build 173 opens the 0.5.2 development line and focuses on orientation-sensor hardware compatibility. The stable 0.5.1 / Build 172 runtime remains the regression baseline.

## ICM-20689

A new external-sensor backend is available through:

```text
IDOTMATRIX_ACCEL_DRIVER_ICM20689
```

The implementation is derived from the WLED sensor code already verified with the user's external module. It:

- probes I2C addresses `0x68` and `0x69` when the configured address is `0`;
- requires `WHO_AM_I=0x98` for the ICM-20689 selection;
- wakes the device with `PWR_MGMT_1=0x01`;
- configures 50 Hz sampling and +/-2 g acceleration;
- configures the ICM-20689 accelerometer DLPF through register `0x1D`;
- reads back critical configuration bits before reporting successful initialization;
- converts raw XYZ samples to normalized acceleration in `g` using the +/-2 g scale.

The common orientation classifier, hysteresis, stability timer, mount compensation and final framebuffer rotation are unchanged.

## MPU-6050 compatibility

The same low-level driver provides an `IDOTMATRIX_ACCEL_DRIVER_MPU6050` selection for devices reporting `WHO_AM_I=0x68/0x69`. This path is implemented for future compatibility but has not been hardware-qualified in the emulator project.

## Qualification profile

Use:

```bash
pio run -e matrixportal_s3_hub75_64_icm20689
```

This environment keeps the normal MatrixPortal 64x64 display configuration, selects the external ICM-20689 backend and enables continuous orientation sample diagnostics.

Expected successful startup includes:

```text
ORIENTATION SENSOR: driver=ICM-20689 init=OK
ORIENTATION AUTO-ROTATE: enabled
```

During physical rotation, verify the normalized sequence expected by the common engine:

```text
+Y -> +X -> -Y -> -X -> +Y
```

If the external module is mounted with a planar offset, set `IDOTMATRIX_ACCEL_MOUNT_ROTATION` to `0`, `90`, `180` or `270` instead of changing the orientation engine.

## External I2C pins

If the target board does not use the required pins as its default Wire bus, define both:

```text
IDOTMATRIX_I2C_SDA_PIN=<gpio>
IDOTMATRIX_I2C_SCL_PIN=<gpio>
```

Defining only one of the two causes a compile-time error.

## Qualification status

- LIS3DH / MatrixPortal S3: **hardware-qualified** from 0.5.1.
- ICM-20689: **implemented; hardware qualification pending at Build 173** (later qualified on ESP32-C3; see Build 176).
- MPU-6050: **implemented; hardware qualification pending**.

No BLE protocol, Graffiti, media, audio, automation or renderer behavior is intentionally changed in Build 173.
