# Build 176 Static Audit

**Release:** `0.5.2-dev`  
**Build:** `176`

## Scope

This audit covers the Build 175 source tree after the successful ESP32-C3 / ICM-20689 hardware test, the Build 176 cleanup diff, repository documentation, Wiki alignment, local hardware configuration behavior and existing static regression tests.

## Confirmed release-state corrections

- ICM-20689 status changed from pending/unqualified to hardware-qualified on the tested ESP32-C3 shared-I2C configuration.
- MPU-6050 remains implemented but unqualified.
- The MatrixPortal external-ICM profile is not represented as separately hardware-qualified.
- Qualification diagnostics introduced in Build 175 are retained but no longer forced in normal builds.
- The sample-diagnostic timestamp is now conditionally compiled, eliminating an unused variable when continuous XYZ logging is disabled.
- PlatformIO profile documentation now matches the actual `IDOTMATRIX_DEFAULT_*` fallback macros.
- Current build/signature references were advanced to Build 176; historical Build 173-175 notes are retained as history.

## Regression boundary

No intentional Build 176 changes were made to BLE protocol handling, Graffiti framing, media transfer, Audio/Rhythm, display ownership, storage semantics or renderer behavior. The sensor-driver register logic is unchanged from the physically successful Build 175 test.

## Remaining qualification gaps

- genuine MPU-6050 hardware;
- MatrixPortal S3 with an external ICM-20689;
- additional boards/sensors not yet physically exercised.
