#pragma once

// -----------------------------------------------------------------------------
// Optional orientation-sensor architecture
//
// Select exactly one accelerometer backend from the build environment. Any
// enabled accelerometer backend automatically enables the common orientation
// subsystem. Boards without a selected backend do not compile any sensor or
// orientation code and do not pull the related library dependencies.
// -----------------------------------------------------------------------------

#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH) && defined(IDOTMATRIX_ACCEL_DRIVER_MPU6050)
  #error "Select only one IDOTMATRIX_ACCEL_DRIVER_* backend"
#endif

#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH) || defined(IDOTMATRIX_ACCEL_DRIVER_MPU6050)
  #define IDOTMATRIX_ORIENTATION_SENSOR 1
#else
  #define IDOTMATRIX_ORIENTATION_SENSOR 0
#endif

#ifndef IDOTMATRIX_ORIENTATION_DIAGNOSTICS
  #define IDOTMATRIX_ORIENTATION_DIAGNOSTICS 0
#endif

// Detailed sample logging is intentionally separate from normal orientation
// diagnostics. Production/development builds can keep transition logs without
// printing X/Y/Z values continuously.
#ifndef IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS
  #define IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS 0
#endif

// Selecting an accelerometer backend enables automatic orientation by default.
// Set this to 0 to keep sensor classification active without rotating output.
#ifndef IDOTMATRIX_ORIENTATION_AUTO_ROTATE
  #define IDOTMATRIX_ORIENTATION_AUTO_ROTATE 1
#endif

// Clockwise planar rotation of the accelerometer PCB relative to the display.
// This is deliberately sensor/board agnostic so the same accelerometer driver
// can be reused on custom hardware or as an external module. Supported values:
// 0, 90, 180, 270. The orientation engine compensates this mounting offset
// before applying the final display rotation.
#ifndef IDOTMATRIX_ACCEL_MOUNT_ROTATION
  #define IDOTMATRIX_ACCEL_MOUNT_ROTATION 0
#endif

#if IDOTMATRIX_ACCEL_MOUNT_ROTATION != 0 && \
    IDOTMATRIX_ACCEL_MOUNT_ROTATION != 90 && \
    IDOTMATRIX_ACCEL_MOUNT_ROTATION != 180 && \
    IDOTMATRIX_ACCEL_MOUNT_ROTATION != 270
  #error "IDOTMATRIX_ACCEL_MOUNT_ROTATION must be 0, 90, 180 or 270"
#endif

#ifndef IDOTMATRIX_ACCEL_SAMPLE_INTERVAL_MS
  #define IDOTMATRIX_ACCEL_SAMPLE_INTERVAL_MS 100UL
#endif

#ifndef IDOTMATRIX_ORIENTATION_STABLE_MS
  #define IDOTMATRIX_ORIENTATION_STABLE_MS 600UL
#endif

#ifndef IDOTMATRIX_ORIENTATION_DIAG_INTERVAL_MS
  #define IDOTMATRIX_ORIENTATION_DIAG_INTERVAL_MS 500UL
#endif

#ifndef IDOTMATRIX_ORIENTATION_AXIS_MIN_G
  #define IDOTMATRIX_ORIENTATION_AXIS_MIN_G 0.55f
#endif

#ifndef IDOTMATRIX_ORIENTATION_AXIS_HYSTERESIS_G
  #define IDOTMATRIX_ORIENTATION_AXIS_HYSTERESIS_G 0.12f
#endif
