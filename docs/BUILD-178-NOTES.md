# Build 178 Development Notes

**Release:** `0.5.2-dev`  
**Build:** `178`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-dev-B178`

## Purpose

Build 178 adds persistent DS3231 real-time-clock support to the ESP32-C3 reference hardware while preserving the already qualified shared-I2C orientation path and passive buzzer implementation.

## Shared-I2C architecture

The ESP32-C3 reference profile now defines:

```text
SDA = GPIO1
SCL = GPIO2
```

The bus is initialized once by the firmware. The RTC backend receives and uses the existing `Wire` object; it never calls `Wire.begin()` itself. This avoids reconfiguring a bus that may also carry the gesture sensor and ICM-20689.

## DS3231 behavior

The first RTC backend is DS3231 at its fixed `0x68` address.

- On boot, a responsive RTC with a valid date/time and clear oscillator-stop flag is accepted immediately.
- A valid RTC has boot-display priority and starts the Clock before stored Carousel fallback.
- Alarm, Program/Schedule and ECO timing can use the RTC without a BLE connection.
- If the oscillator-stop flag is set or the stored date/time is invalid, the RTC is not trusted.
- The official-app `01/80` time-sync command updates the software clock as before and, by default, also writes the DS3231 and clears the oscillator-stop flag.
- After synchronization, time remains available across MCU reboot while RTC backup power is maintained.

## Address interaction with ICM-20689 / MPU-family sensors

DS3231 is fixed at `0x68`. Therefore an MPU-family accelerometer sharing the same physical I2C bus must use `0x69`.

Build 178:

- rejects an explicit `IDOTMATRIX_ACCEL_I2C_ADDRESS=0x68` when DS3231 and an MPU-family backend are both enabled;
- keeps address `0` auto-probe available;
- changes that auto-probe order to `0x69` then `0x68` while the DS3231 backend is enabled.

A real electrical address collision cannot be solved in software: the accelerometer must be strapped to `0x69` when a DS3231 is present.

## Configuration

The ESP32-C3 PlatformIO profile enables:

```text
IDOTMATRIX_DEFAULT_I2C_SDA_PIN=1
IDOTMATRIX_DEFAULT_I2C_SCL_PIN=2
IDOTMATRIX_DEFAULT_RTC_TYPE=DS3231
IDOTMATRIX_DEFAULT_RTC_I2C_ADDRESS=0x68
IDOTMATRIX_DEFAULT_RTC_SYNC_FROM_BLE=1
```

All values remain overridable through `src/IDotMatrixUserConfig.h`.

## Qualification boundary

Static RTC/configuration tests pass, including shared-bus defaults, BLE-sync integration, DS3231/MPU address-conflict rejection and direct-driver syntax checks. The previously qualified ICM-20689 and passive-buzzer behavior is intentionally unchanged.

Physical DS3231 qualification on the complete ESP32-C3 shared-bus assembly is still required.
