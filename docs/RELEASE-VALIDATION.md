# 0.6.0-rc.1 Release Validation

## Identity gate

The publication candidate must report:

```text
IDOTMATRIX_FW=0.6.0-rc.1
```

## Automated repository gate

From the repository root:

```bash
python3 tests/run_tests.py
bash -n update_idotmatrix_emulator.sh
```

The package must not contain `.pio`, `.pytest_cache`, `__pycache__`, `.pyc`, generated firmware or a local `src/IDotMatrixUserConfig.h`.

## Build gate

Compile the intended publication target:

```bash
pio run -e waveshare_s3_rgbmatrix_64x64
```

For full cross-target assurance, compile the remaining checked-in environments before final promotion.

## Waveshare physical smoke gate

1. Confirm normal BLE discovery and connection from the official app.
2. Confirm the connection beep is audible.
3. Run a Countdown and confirm the completion trill.
4. Exercise Program/Schedule notification.
5. Exercise an Alarm and confirm repeated trill behavior and correct stop behavior.
6. Confirm normal PNG/GIF/TEXT Carousel playback, including TEXT returning to the next slot.
7. Replace a Carousel repeatedly and confirm stable BLE operation with no spontaneous reboot.
8. Briefly press BOOT and confirm a software reboot while the serial monitor remains usable.
9. Hold BOOT for at least two seconds and confirm OTA maintenance mode only when intentionally testing OTA.

## Regression expectations

The RC must preserve existing Clock, TEXT, image/GIF, Graffiti, Preset, Alarm, Program/Schedule, timers, Audio/Rhythm visualization, RTC, orientation and non-Waveshare GPIO-buzzer behavior.

## Final promotion gate

After the automated, build and physical smoke gates pass:

1. update the public identity from `0.6.0-rc.1` to `0.6.0` only;
2. do not change qualified runtime behavior during that promotion;
3. rerun the automated gate;
4. compile the publication target again;
5. tag the final tree as `v0.6.0`.
