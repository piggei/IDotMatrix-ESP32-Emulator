# iDotMatrix ESP32 Emulator 0.5.2-rc.3

**Release:** `0.5.2-rc.3`  
**Build:** `185`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-rc.3-B185`

## Purpose

RC3 is a narrowly scoped electrical-idle correction for the passive buzzer path introduced earlier in the 0.5.2 line. The qualified ESP32-C3 buzzer is a three-wire transistor-driven passive module marked `low level trigger`, powered from 3.3 V.

## Passive buzzer idle fix

The C3 reference profile now sets:

```text
GPIO: 3
Tone: 2000 Hz
Passive trigger: LOW
Silent idle level: HIGH
```

While a beep is active, the ESP32 LEDC peripheral generates the same 50% square-wave tone as before. While silent, the firmware drives the output to the inactive logic level rather than leaving a low-level-trigger module continuously biased by a constant LOW signal.

The new `IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW` setting allows the same backend to support both low-level-trigger modules and direct/active-high passive buzzers.

## Unchanged areas

No other runtime behavior is intentionally changed from RC2/B184. Carousel boot priority, DS3231 RTC behavior, Clock presentation persistence, BLE protocol handling, Graffiti, Bulk/media, Audio/Rhythm and orientation backends remain unchanged.

## Validation

Static regression tests verify C3 profile polarity, configuration resolution, safe idle code paths and the existing buzzer event policy. The final RC3 hardware smoke test should confirm that the module remains cool/silent at idle and that Alarm, Countdown, Schedule and BLE connection notifications still sound correctly.
