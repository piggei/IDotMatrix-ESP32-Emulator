# Build 177 Development Notes

**Release:** `0.5.2-dev`  
**Build:** `177`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-dev-B177`

## Purpose

Build 177 adds passive-buzzer support for the ESP32-C3 reference hardware after the active buzzer on GPIO3 was physically replaced by a passive device.

## Implementation

- `IDOTMATRIX_BUZZER_NONE`, `IDOTMATRIX_BUZZER_ACTIVE` and `IDOTMATRIX_BUZZER_PASSIVE` select the electrical/output backend.
- Passive output uses Arduino-ESP32 3.x LEDC: the GPIO is attached once and `ledcWriteTone()` starts/stops the square wave.
- The existing non-blocking three-pulse state machine is retained for Alarm, Countdown and Program/Schedule; the BLE connection notification remains one short pulse.
- The ESP32-C3 16x16 profile defaults to passive GPIO3 at 2000 Hz and enables all four notification policies.
- Active buzzer support remains available for other boards.
- `IDotMatrixUserConfig.h` can override type, pin, frequency, active polarity and every event policy.

## Qualification boundary

The source and configuration paths are statically validated in this build. The passive buzzer output on the ESP32-C3 was subsequently physically tested and confirmed working. This document retains the Build 177 implementation boundary; the qualification result is recorded here for historical accuracy. BLE, display, media and orientation code paths are intentionally unchanged.
