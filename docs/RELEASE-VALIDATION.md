# 0.6.0 Release Validation

## Identity gate

The release firmware must report:

```text
IDOTMATRIX_FW=0.6.0
```

The internal diagnostic build identifier is separate from the public release identity.

## Automated repository gate

From the repository root:

```bash
python3 tests/run_tests.py
bash -n update_idotmatrix_emulator.sh
```

The release package must not contain `.pio`, `.pytest_cache`, `__pycache__`, `.pyc`, generated firmware or a local `src/IDotMatrixUserConfig.h`.

## Build gate

Compile the primary publication target:

```bash
pio run -e waveshare_s3_rgbmatrix_64x64
```

For full checked-in cross-target assurance, compile all published environments:

```bash
pio run -e waveshare_s3_rgbmatrix_16x16
pio run -e waveshare_s3_rgbmatrix_32x32
pio run -e waveshare_s3_rgbmatrix_64x64
pio run -e matrixportal_s3_hub75_64
pio run -e matrixportal_s3_hub75_64_icm20689
pio run -e esp32c3_ws2812_16
pio run -e ios_compat_esp32_ws2812_32
```

The iOS environment is diagnostic rather than part of the main physical release gate, but compiling it protects the checked-in compatibility branch from source regressions.

## Waveshare physical smoke gate

1. Confirm normal BLE discovery and connection from the official app.
2. Confirm the connection notification sound.
3. Run a Countdown and confirm the completion trill.
4. Exercise Program/Schedule notification.
5. Exercise an Alarm and confirm repeated trill behavior and correct stop behavior.
6. Confirm normal PNG/GIF/TEXT Carousel playback, including TEXT returning to the next slot.
7. Send complete Carousel banks consecutively without rebooting/disconnecting. Each transaction must reach `[CARBANK] state=commit`, the new bank must become usable, and no `Stack canary watchpoint triggered (nimble_host)` or spontaneous reboot may occur.
8. While Preset/Default playback is active, send a new Carousel bank. After `[CARBANK] state=commit` and the settle boundary, confirm Preset/Default stops and the first valid slot of the new Carousel starts automatically without requiring another Assets-view command.
9. Include a mixed PNG/GIF/TEXT bank so all persistent receive types exercise the PSRAM-buffered path.
10. Interrupt a Carousel replacement before commit and confirm the previous complete bank is recovered after reboot.
11. Interrupt a Preset replacement by disconnecting BLE and confirm the previous Preset resumes intact.
12. Briefly press BOOT and confirm a software reboot while the serial monitor remains usable.
13. Hold BOOT for at least two seconds and confirm OTA maintenance mode only when intentionally testing OTA.

## Recorded hardware evidence

The final Waveshare qualification includes three consecutive complete Carousel replacements with successful commit/playback and no Guru Meditation, `nimble_host` stack-canary or spontaneous reboot. Static PNG playback and subsequent GIF playback were observed after commit, confirming the final buffered receive/publication boundary on physical hardware. The Preset/Default-to-Carousel ownership transfer remains an explicit final hardware gate for this updated package.

## Regression expectations

The release must preserve existing Clock, TEXT, image/GIF, Graffiti, Preset, Alarm, Program/Schedule, timers, Audio/Rhythm visualization, RTC, orientation and non-Waveshare GPIO-buzzer behavior.

## Publication

After the automated, build and physical gates pass, tag the final tree as `v0.6.0`. Do not change qualified runtime behavior during packaging.
