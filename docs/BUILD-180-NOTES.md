# Build 180 Development Notes

**Release:** `0.5.2-dev`  
**Build:** `180`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-dev-B180`

## Purpose

Build 180 fixes ESP32-C3 native-USB serial routing in the PlatformIO reference profile and adds late RTC/boot diagnostics. It intentionally does not change DS3231 timekeeping or boot-selection logic.

## ESP32-C3 serial routing

The `esp32c3_ws2812_16` environment now defines:

```text
ARDUINO_USB_MODE=1
ARDUINO_USB_CDC_ON_BOOT=1
```

This routes Arduino `Serial` to the ESP32-C3 native USB CDC/JTAG connection used by the reference hardware.

For Arduino IDE, select **USB CDC On Boot = Enabled** and **USB Mode = Hardware CDC and JTAG** when monitoring through native USB.

## RTC startup summary

At the end of `setup()` and twice again after startup, the firmware prints a compact summary containing:

- firmware signature and MCU family;
- whether `IDotMatrixUserConfig.h` was included;
- resolved shared-I2C pins;
- RTC backend and address;
- `rtcReady`, `rtcTimeValid` and DS3231 status register;
- current RTC date/time when readable;
- software BLE-time-sync state;
- boot-selected display mode and screen-power state.

This is specifically intended to determine why a battery-backed DS3231 may contain the correct time after BLE synchronization yet fail to select Clock on the next cold boot.

## Important Arduino IDE distinction

PlatformIO environment `DEFAULT_*` flags are not present in an Arduino IDE build. Therefore RTC/I2C/buzzer settings that rely on the `esp32c3_ws2812_16` profile must be copied explicitly into `src/IDotMatrixUserConfig.h` when compiling the same source directly with Arduino IDE.

## RTC logic unchanged

The existing DS3231 `adjust()` path already clears the oscillator-stop flag after a successful BLE time synchronization. Build 180 therefore does not add another OSF-clear operation. Boot Clock remains selected only when the RTC initializes successfully and its time is valid.
