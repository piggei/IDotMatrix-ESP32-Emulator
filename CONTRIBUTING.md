# Contributing

Contributions are welcome, especially from owners of original iDotMatrix hardware.

This project was reverse-engineered primarily by observing the official app and implementing an ESP32 emulator. Because we do not currently have an original iDotMatrix device available for comparison, independent observations from real hardware are particularly valuable.

## How you can help

You do not need to write code. Useful contributions include:

- BLE captures from an original iDotMatrix device.
- Unknown BLE commands and the exact app action that produced them.
- Tests with different iDotMatrix models and app versions.
- Corrections or confirmations for `PROTOCOL.md`.
- Clear photographs of the original PCB, front and back.
- Identification of the MCU, buzzer circuitry, RTC or other components.
- ESPHome, Home Assistant, Python or other independent implementations of the documented protocol.
- Firmware bug reports and reproducible test cases.

## Protocol findings

When reporting a protocol finding, please include as much of the following as possible:

1. iDotMatrix model, if known.
2. App version and operating system.
3. Exact action performed in the app.
4. Direction of the packet (`App -> device` or `Device -> app`).
5. Complete raw packet in hexadecimal when available.
6. Observed behavior.
7. Whether the result was reproduced more than once.

Please do not modify historical raw captures to match a current interpretation. Raw captures are experimental evidence; interpretations belong in `PROTOCOL.md`.

## Unknown commands

For an unknown command, open the **Unknown BLE command** issue template. The ESP32 emulator can display the first bytes of an unhandled command on its onboard OLED, even when a serial console is not available.

## Pull requests

Keep protocol changes separate from unrelated refactoring where practical. If a pull request changes a protocol interpretation, please include the evidence that supports the change and update `PROTOCOL.md`, `TODO.md` and `HISTORY.md` when appropriate.

## License

By contributing to this repository, you agree that your contribution may be distributed under the repository's MIT License.
