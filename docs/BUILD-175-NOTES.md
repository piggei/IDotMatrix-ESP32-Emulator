# Build 175 Development Notes

**Release:** `0.5.2-dev`  
**Build:** `175`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-dev-B175`

## Purpose

Build 175 is a diagnostic follow-up to Build 174 for qualification of an external ICM-20689 on ESP32-C3 hardware where the I2C bus is shared with another sensor. The same physical sensor/bus arrangement is already known to work in the related WLED implementation, so this build deliberately avoids changing sensor register programming or orientation behavior before the failure point is identified.

## Diagnostic changes

When an accelerometer backend is selected, one-shot orientation diagnostics are enabled by default. For the ICM-20689 / MPU-family backend the firmware records and reports each relevant probe independently:

```text
ACCEL PROBE 0x68: ACK=YES WHO_AM_I=0x98 MATCH=YES
ACCEL PROBE 0x69: not attempted
ACCEL RESULT: ready=YES activeAddress=0x68 who=0x98 config=OK
```

Failure cases distinguish no ACK, `WHO_AM_I` read failure, identity mismatch and post-probe configuration failure.

The complete summary is printed again near the end of `setup()`. This is intentional: on ESP32-C3 USB/CDC the host serial monitor can become usable only after the earliest boot-time sensor messages have already been emitted.

The summary also reports whether explicit SDA/SCL pins were supplied through `IDotMatrixUserConfig.h` or the board defaults are being used.

## Local configuration

Continuous sample output remains optional:

```cpp
#define IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS 1
```

Normal one-shot diagnostics can be silenced explicitly with:

```cpp
#define IDOTMATRIX_ORIENTATION_DIAGNOSTICS 0
```

## Functional boundary

Build 175 does **not** intentionally change:

- I2C bus initialization semantics;
- ICM-20689 or MPU-6050 register programming;
- I2C address selection logic;
- the common orientation classifier;
- display rotation mapping;
- BLE, Graffiti, media, audio or display-rendering paths.

The build exists to obtain authoritative hardware evidence before any shared-bus behavior is changed.


## Subsequent qualification outcome

Physical testing of this build succeeded on ESP32-C3 with the ICM-20689 sharing the I2C bus with the gesture sensor. Build 176 records that qualification result and returns verbose diagnostics to opt-in operation.
