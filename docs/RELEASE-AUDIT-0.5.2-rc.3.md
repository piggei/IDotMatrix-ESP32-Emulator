# 0.5.2-rc.3 Release Audit

**Release:** `0.5.2-rc.3`  
**Build:** `185`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-rc.3-B185`

## Scope

This audit covers the RC3 change from the hardware-tested RC2/B184 baseline. The requested correction is limited to the idle electrical state of the qualified three-wire passive buzzer module on ESP32-C3 GPIO3, powered from 3.3 V.

## Source review

- Added configurable `IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW`.
- ESP32-C3 profile sets the qualified low-level-trigger module to `1`.
- Passive tone generation remains LEDC-based at 2000 Hz.
- Silent state uses 100% LEDC duty (constant HIGH) for low-level-trigger modules and 0% duty (constant LOW) for active-high/direct passive buzzers.
- The inactive GPIO level is established before LEDC attachment.
- Active buzzer behavior and its existing polarity setting are unchanged.
- No intentional change was made to RTC, Carousel, Clock persistence, BLE, Graffiti, media, Audio/Rhythm or orientation behavior.

## Documentation review

README, hardware support/configuration, PlatformIO guidance, release validation, history and Wiki were updated to describe the three-wire `VCC/GND/I/O` low-level-trigger reference module and its HIGH idle state.

## Static gates

The repository static suite, buzzer configuration tests, release/build identity checks, preprocessor balance and documentation-link checks must pass from the packaged source tree.

## Hardware gate

Before promotion, verify on the ESP32-C3 reference unit that the passive module remains silent and does not warm at idle, while the existing audible notification patterns still operate correctly.

## Verification performed for packaging

- all repository `tests/test_*.py`: **PASS**;
- passive buzzer configuration/default/override regression: **PASS**;
- preprocessor directive balance: **PASS**;
- release/build identity checks: **PASS**;
- source and Wiki Markdown/image link checks: **PASS**;
- isolated C++ compile of the passive idle logic with both low-trigger and active-high configurations under `-Wall -Wextra -Werror`: **PASS**;
- runtime diff review against RC2/B184: limited to version/build identifiers, passive buzzer trigger-polarity configuration, passive idle output behavior, tests and documentation.

PlatformIO is not installed in the packaging environment, so a full ESP32 toolchain build was not executed here. The on-device RC3 smoke test remains required.
