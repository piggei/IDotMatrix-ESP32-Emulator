# 0.5.2-rc.2 Release Audit

**Release:** `0.5.2-rc.2`  
**Build:** `184`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-rc.2-B184`

## Scope

RC2 is based directly on RC1/B183 and contains one intentional runtime correction: persisted Carousel content has boot-display priority over the optional RTC-backed Clock.

## Source change

`applyBootDisplayPolicy()` now evaluates the persisted Carousel before the RTC branch. If Carousel startup fails or no valid slot exists, a valid RTC selects Clock. With neither available, the screen remains off.

A new `tests/test_boot_display_policy.py` regression test verifies the source ordering and associated diagnostic messages.

## Documentation

README, protocol documentation, release validation, original-hardware reference, release notes and Wiki current-state pages were aligned with the RC2 boot policy. RC1/B183 remains in historical sections only.

## Runtime boundary

No intentional changes were made to BLE services/commands, Graffiti framing, normal Bulk, rendering, Clock NVS persistence, RTC driver/recovery, LittleFS media ownership, Audio/Rhythm, orientation or buzzer behavior.

## Publication gate

Before publishing RC2 binaries, perform at minimum:

- cold boot with a persisted Carousel and valid RTC: Carousel must start;
- cold boot without a persisted Carousel and with valid RTC: Clock must start with persisted presentation;
- cold boot without Carousel and without valid RTC: screen must remain off;
- representative BLE reconnect and normal media smoke test.
