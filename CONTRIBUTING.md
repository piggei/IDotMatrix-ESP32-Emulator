# Contributing

Contributions are welcome, especially protocol captures, hardware observations, reproducible regressions and tests on additional iDotMatrix models.

The project combines official-app reverse engineering with direct comparison against an original 64x64 iDotMatrix. New evidence should therefore be clearly labeled as one of:

- direct original-hardware observation;
- official-app/emulator capture;
- emulator policy;
- inference or third-party corroboration.

## How you can help

Useful contributions include:

- BLE captures from original iDotMatrix hardware;
- unknown commands and the exact app action that produced them;
- tests with different display models, firmware versions and app versions;
- corrections or confirmations for `PROTOCOL.md`;
- photographs and component identification from original hardware;
- reproducible firmware bug reports;
- independent client/server implementations that corroborate packet semantics.

## Protocol findings

When reporting a protocol finding, include as much of the following as possible:

1. iDotMatrix model and firmware version, if known.
2. App version and operating system.
3. Exact action performed in the app.
4. Packet direction (`App -> device` or `Device -> app`).
5. Complete raw packet in hexadecimal when available.
6. Observed device/app behavior.
7. Whether the result was reproduced more than once.

Raw captures are evidence and should not be rewritten to fit a later interpretation. Interpretations belong in `PROTOCOL.md` and should state their confidence level.

## Pull requests

Keep protocol changes separate from unrelated refactoring where practical. If a pull request changes a protocol interpretation, include the evidence supporting it and update the relevant documentation.

Build history belongs in `HISTORY.md`; the project overview in `README.md` should remain presentation-oriented.

## License

By contributing to this repository, you agree that your contribution may be distributed under the repository's MIT License.
