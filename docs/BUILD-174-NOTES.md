# Build 174 Development Notes

**Release:** `0.5.2-dev`  
**Build:** `174`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-dev-B174`

## Purpose

Build 174 keeps the Build 173 ICM-20689 / MPU-family implementation unchanged and adds a persistent local hardware-configuration layer for sensor qualification on different boards and wiring layouts.

## Optional local include

Copy:

```text
src/IDotMatrixUserConfig.example.h
```

to:

```text
src/IDotMatrixUserConfig.h
```

The local file may define:

```text
IDOTMATRIX_ACCEL_DRIVER_LIS3DH
IDOTMATRIX_ACCEL_DRIVER_ICM20689
IDOTMATRIX_ACCEL_DRIVER_MPU6050
IDOTMATRIX_I2C_SDA_PIN
IDOTMATRIX_I2C_SCL_PIN
IDOTMATRIX_ACCEL_I2C_ADDRESS
IDOTMATRIX_ACCEL_MOUNT_ROTATION
```

Explicit local values take precedence over the corresponding PlatformIO profile defaults.

## PlatformIO fallback model

Sensor-related build flags in the checked-in environments now use `IDOTMATRIX_DEFAULT_*` names. `IDotMatrixHardwareConfig.h` applies them only when the local header has not supplied an explicit value. This prevents duplicate sensor selections while preserving the existing behavior when no local header exists.

## Update-helper persistence

`update_idotmatrix_emulator.sh` now preserves `src/IDotMatrixUserConfig.h` during `rsync --delete` synchronization. The actual local file is ignored by Git; only the `.example.h` template is part of the repository/release archive.

## Runtime scope

No BLE protocol, Graffiti, media, audio, display renderer, orientation-classification algorithm or accelerometer register logic is intentionally changed in Build 174.
