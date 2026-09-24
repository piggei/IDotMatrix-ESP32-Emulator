# Build 176 Development Notes

**Release:** `0.5.2-dev`  
**Build:** `176`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-dev-B176`

## Purpose

Build 176 consolidates the successful Build 175 ESP32-C3 / ICM-20689 hardware test and removes qualification-only logging from normal builds. No protocol or renderer change is intended.

## Hardware qualification update

The ICM-20689 path is now hardware-qualified on the tested ESP32-C3 configuration with the I2C bus shared with the gesture sensor. Qualification confirmed that the existing auto-probe/register path and common orientation engine operate correctly on the emulator hardware.

The MPU-6050 compatibility path remains implemented but unqualified because no genuine MPU-6050 module has yet been tested in this project. The `matrixportal_s3_hub75_64_icm20689` environment remains a useful external-sensor build profile, but that specific MatrixPortal/external-ICM combination has not been separately exercised.

## Cleanup

- verbose `IDOTMATRIX_ORIENTATION_DIAGNOSTICS` now defaults to `0`;
- continuous XYZ diagnostics remain opt-in;
- normal builds emit one concise serial error if an enabled orientation sensor fails to initialize;
- the detailed Build 175 probe report is retained for targeted troubleshooting;
- PlatformIO profiles no longer force qualification logging;
- the sample-diagnostic timestamp is compiled only when XYZ sample diagnostics are enabled, removing an otherwise-unused variable from clean builds;
- README, hardware-support documents and Wiki qualification status are aligned with the physical test result.

## Functional boundary

Build 176 intentionally leaves unchanged:

- ICM-20689 and MPU-6050 register programming;
- `0x68/0x69` address auto-probe;
- common orientation classification, hysteresis and mount compensation;
- BLE protocol and original-hardware Graffiti framing;
- Audio/Rhythm transport;
- media storage, Carousel, Preset, Alarm and Program/Schedule behavior;
- logical/physical display mapping apart from the already-qualified orientation transform.
