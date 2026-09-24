#pragma once

// -----------------------------------------------------------------------------
// Optional local hardware configuration
//
// Copy IDotMatrixUserConfig.example.h to IDotMatrixUserConfig.h and edit the
// copy to keep board-specific wiring outside the tracked source files. The
// local header is optional and, when present, its explicit settings take
// precedence over PlatformIO profile defaults.
// -----------------------------------------------------------------------------
#if __has_include("IDotMatrixUserConfig.h")
  #include "IDotMatrixUserConfig.h"
  #define IDOTMATRIX_USER_CONFIG_PRESENT 1
#else
  #define IDOTMATRIX_USER_CONFIG_PRESENT 0
#endif

// PlatformIO profiles provide DEFAULT_* values rather than hard overrides.
// This lets IDotMatrixUserConfig.h select a different backend/address/mount
// without editing platformio.ini or causing duplicate driver selections.
#if (defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH) && defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_ICM20689)) || \
    (defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH) && defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_MPU6050)) || \
    (defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_ICM20689) && defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_MPU6050))
  #error "Select only one IDOTMATRIX_DEFAULT_ACCEL_DRIVER_* fallback"
#endif

#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH) || \
    defined(IDOTMATRIX_ACCEL_DRIVER_ICM20689) || \
    defined(IDOTMATRIX_ACCEL_DRIVER_MPU6050)
  #define IDOTMATRIX_ACCEL_DRIVER_EXPLICIT 1
#else
  #define IDOTMATRIX_ACCEL_DRIVER_EXPLICIT 0
#endif

#if !IDOTMATRIX_ACCEL_DRIVER_EXPLICIT
  #if defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH)
    #define IDOTMATRIX_ACCEL_DRIVER_LIS3DH
  #elif defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_ICM20689)
    #define IDOTMATRIX_ACCEL_DRIVER_ICM20689
  #elif defined(IDOTMATRIX_DEFAULT_ACCEL_DRIVER_MPU6050)
    #define IDOTMATRIX_ACCEL_DRIVER_MPU6050
  #endif
#endif

// A profile address belongs to its profile-selected backend. If the user
// explicitly switches backend but does not specify an address, let that
// backend use its own safe default (LIS3DH=0x19, MPU family=auto-probe).
#ifndef IDOTMATRIX_ACCEL_I2C_ADDRESS
  #if !IDOTMATRIX_ACCEL_DRIVER_EXPLICIT && defined(IDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS)
    #define IDOTMATRIX_ACCEL_I2C_ADDRESS IDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS
  #endif
#endif

#ifndef IDOTMATRIX_ACCEL_MOUNT_ROTATION
  #ifdef IDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION
    #define IDOTMATRIX_ACCEL_MOUNT_ROTATION IDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION
  #else
    #define IDOTMATRIX_ACCEL_MOUNT_ROTATION 0
  #endif
#endif

// -----------------------------------------------------------------------------
// Optional orientation-sensor architecture
//
// Select exactly one accelerometer backend explicitly or let the selected
// build profile provide a fallback. Any enabled accelerometer backend
// automatically enables the common orientation subsystem. Boards without a
// selected backend do not compile any sensor or orientation code.
// -----------------------------------------------------------------------------

#if (defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH) && defined(IDOTMATRIX_ACCEL_DRIVER_ICM20689)) || \
    (defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH) && defined(IDOTMATRIX_ACCEL_DRIVER_MPU6050)) || \
    (defined(IDOTMATRIX_ACCEL_DRIVER_ICM20689) && defined(IDOTMATRIX_ACCEL_DRIVER_MPU6050))
  #error "Select only one IDOTMATRIX_ACCEL_DRIVER_* backend"
#endif

#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH) || \
    defined(IDOTMATRIX_ACCEL_DRIVER_ICM20689) || \
    defined(IDOTMATRIX_ACCEL_DRIVER_MPU6050)
  #define IDOTMATRIX_ORIENTATION_SENSOR 1
#else
  #define IDOTMATRIX_ORIENTATION_SENSOR 0
#endif

// Optional shared I2C pin override for external sensors/RTC. Define both pins
// or neither; board defaults are used when the override is absent.
#if defined(IDOTMATRIX_I2C_SDA_PIN) != defined(IDOTMATRIX_I2C_SCL_PIN)
  #error "Define both IDOTMATRIX_I2C_SDA_PIN and IDOTMATRIX_I2C_SCL_PIN, or neither"
#endif

#ifndef IDOTMATRIX_ORIENTATION_DIAGNOSTICS
  // Normal builds keep verbose sensor diagnostics disabled. A failed sensor
  // initialization still emits one concise error line; enable this flag for
  // the full probe/configuration report used during hardware qualification.
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
