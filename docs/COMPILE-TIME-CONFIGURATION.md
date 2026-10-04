# Compile-Time Configuration Reference

This page collects the supported compile-time configuration controls for iDotMatrix ESP32 Emulator `0.6.0` in one place.

For normal builds, select the closest checked-in PlatformIO environment first. Use `src/IDotMatrixUserConfig.h` only for hardware-specific overrides that should survive repository updates. Advanced media/performance flags are normally managed in `platformio.ini` and should be changed only when deliberately re-qualifying a target.

## Configuration precedence

For hardware options implemented through `IDotMatrixHardwareConfig.h`, precedence is:

1. explicit `IDOTMATRIX_*` values in `src/IDotMatrixUserConfig.h`;
2. profile-provided `IDOTMATRIX_DEFAULT_*` values in `platformio.ini`;
3. source defaults.

Create the local override file with:

```bash
cp src/IDotMatrixUserConfig.example.h src/IDotMatrixUserConfig.h
```

The local file is ignored by Git, excluded from release packages and preserved by `update_idotmatrix_emulator.sh`.

## Display and profile selection

These options are normally profile-owned rather than local user overrides.

| Macro | Meaning | Profile values |
| --- | --- | --- |
| `DISPLAY_BACKEND` | Physical output backend | HUB75 or WS2812/FastLED |
| `IDOTMATRIX_SCREEN_TYPE` | Logical identity exposed to the app | `1` = 16x16, `3` = 32x32, `4` = 64x64 |
| `PHYSICAL_MATRIX_WIDTH` | Physical output width | Waveshare/MatrixPortal: `64` |
| `PHYSICAL_MATRIX_HEIGHT` | Physical output height | Waveshare/MatrixPortal: `64` |
| `MATRIX_PIN` | WS2812 data GPIO | generic default `4`; profile-specific overrides may apply |

Logical and physical resolution are intentionally independent. Do not change the screen type merely to match a panel size; select the logical iDotMatrix identity required by the official app.

## Accelerometer and orientation

### Backend and wiring

| Macro | Meaning | Default/policy |
| --- | --- | --- |
| `IDOTMATRIX_ACCEL_DRIVER_NONE` | Explicitly disable a profile accelerometer fallback | off |
| `IDOTMATRIX_ACCEL_DRIVER_LIS3DH` | Select LIS3DH backend | profile/local |
| `IDOTMATRIX_ACCEL_DRIVER_ICM20689` | Select ICM-20689 backend | profile/local |
| `IDOTMATRIX_ACCEL_DRIVER_MPU6050` | Select MPU-6050-compatible path | profile/local |
| `IDOTMATRIX_I2C_SDA_PIN` | Shared external I2C SDA | define with SCL or neither |
| `IDOTMATRIX_I2C_SCL_PIN` | Shared external I2C SCL | define with SDA or neither |
| `IDOTMATRIX_ACCEL_I2C_ADDRESS` | Sensor address | LIS3DH normally `0x19`; MPU family `0` = probe `0x68/0x69` |
| `IDOTMATRIX_ACCEL_MOUNT_ROTATION` | Clockwise sensor-board mounting compensation | `0`, `90`, `180`, `270` |

Only one real accelerometer backend may be selected. `IDOTMATRIX_ACCEL_DRIVER_NONE` cannot be combined with a real backend.

### Orientation behavior and qualification tuning

| Macro | Default | Meaning |
| --- | ---: | --- |
| `IDOTMATRIX_ORIENTATION_AUTO_ROTATE` | `1` | Apply classified orientation to the display; set `0` to keep sensor classification without rotating output |
| `IDOTMATRIX_ACCEL_SAMPLE_INTERVAL_MS` | `100` | Sensor sampling interval |
| `IDOTMATRIX_ORIENTATION_STABLE_MS` | `600` | Required stable orientation time before accepting a change |
| `IDOTMATRIX_ORIENTATION_AXIS_MIN_G` | `0.55f` | Minimum dominant-axis magnitude |
| `IDOTMATRIX_ORIENTATION_AXIS_HYSTERESIS_G` | `0.12f` | Axis-switch hysteresis |
| `IDOTMATRIX_ORIENTATION_DIAGNOSTICS` | `0` | Detailed probe/configuration logging |
| `IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS` | `0` | Continuous XYZ sample logging |
| `IDOTMATRIX_ORIENTATION_DIAG_INTERVAL_MS` | `500` | Diagnostic sample/report interval |

The timing/threshold values above are engineering tuning controls. Changing them creates a new behavior that should be physically re-qualified.

## RTC

| Macro | Default/policy | Meaning |
| --- | --- | --- |
| `IDOTMATRIX_RTC_TYPE` | profile default or `IDOTMATRIX_RTC_NONE` | RTC backend selection |
| `IDOTMATRIX_RTC_I2C_ADDRESS` | `0x68` for DS3231 | RTC address |
| `IDOTMATRIX_RTC_SYNC_FROM_BLE` | profile/source policy | Write valid app time sync to the RTC |
| `IDOTMATRIX_RTC_RETRY_INTERVAL_MS` | `60000UL` on qualified C3 profile | Runtime re-detection interval; minimum 1000 ms |
| `IDOTMATRIX_RTC_DIAGNOSTICS` | `0` | Verbose RTC diagnostics |

Supported RTC constants are `IDOTMATRIX_RTC_NONE` and `IDOTMATRIX_RTC_DS3231`. DS3231 uses fixed address `0x68`; an MPU-family accelerometer on the same bus must therefore use `0x69`.

## GPIO buzzer and notification policy

| Macro | Default/policy | Meaning |
| --- | --- | --- |
| `IDOTMATRIX_BUZZER_TYPE` | profile or `IDOTMATRIX_BUZZER_NONE` | `NONE`, `ACTIVE`, or `PASSIVE` |
| `IDOTMATRIX_BUZZER_PIN` | profile or `-1` | Buzzer GPIO |
| `IDOTMATRIX_BUZZER_FREQUENCY_HZ` | `2000` | Passive tone frequency and synthesized notification reference frequency |
| `IDOTMATRIX_BUZZER_ACTIVE_HIGH` | `1` | Active-buzzer polarity |
| `IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW` | `0` generic; C3 profile uses `1` | Three-wire passive-module trigger polarity |
| `IDOTMATRIX_ALARM_BUZZER_ENABLED` | profile policy | Alarm notification |
| `IDOTMATRIX_COUNTDOWN_BUZZER_ENABLED` | profile policy | Countdown-complete notification |
| `IDOTMATRIX_SCHEDULE_BUZZER_ENABLED` | profile policy | Program/Schedule notification |
| `IDOTMATRIX_CONNECTION_BUZZER_ENABLED` | profile policy | BLE connection notification |

The qualified ESP32-C3 module uses `PASSIVE`, GPIO3, 2000 Hz and `IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW=1`.

## Waveshare ES8311 / I2S synthesized audio

The Waveshare profiles enable the codec backend by default. Tones are synthesized at runtime; no WAV/PCM files are stored.

| Macro | Default | Meaning |
| --- | ---: | --- |
| `IDOTMATRIX_AUDIO_CODEC_ENABLED` | `1` on Waveshare, otherwise `0` | Enable codec backend |
| `IDOTMATRIX_AUDIO_CODEC_I2C_ADDRESS` | `0x18` | ES8311 control address |
| `IDOTMATRIX_AUDIO_I2C_SDA_PIN` | `47` | Dedicated audio I2C1 SDA |
| `IDOTMATRIX_AUDIO_I2C_SCL_PIN` | `48` | Dedicated audio I2C1 SCL |
| `IDOTMATRIX_AUDIO_I2S_MCLK_PIN` | `12` | I2S1 MCLK |
| `IDOTMATRIX_AUDIO_I2S_BCLK_PIN` | `43` | I2S1 BCLK |
| `IDOTMATRIX_AUDIO_I2S_WS_PIN` | `38` | I2S1 WS/LRCK |
| `IDOTMATRIX_AUDIO_I2S_DOUT_PIN` | `21` | I2S1 TX data |
| `IDOTMATRIX_AUDIO_PA_ENABLE_PIN` | `11` | External amplifier enable |
| `IDOTMATRIX_AUDIO_CODEC_VOLUME` | `100` | Codec volume, valid range `0..100` |

Changing the pin map is intended for custom hardware; the values above are the hardware-qualified Waveshare mapping.

## OTA maintenance

| Macro | Default/policy | Meaning |
| --- | --- | --- |
| `IDOTMATRIX_OTA_ENABLED` | `1` on Waveshare, otherwise `0` | Compile maintenance OTA support |
| `IDOTMATRIX_OTA_TRIGGER_PIN` | GPIO0 on Waveshare | Physical trigger |
| `IDOTMATRIX_OTA_TRIGGER_ACTIVE_LOW` | `1` | Trigger polarity |
| `IDOTMATRIX_OTA_TRIGGER_HOLD_MS` | `2000UL` | Long-press threshold for OTA mode |
| `IDOTMATRIX_OTA_TRIGGER_DEBOUNCE_MS` | `40UL` | BOOT-button debounce |
| `IDOTMATRIX_OTA_AP_PASSWORD` | `"idotmatrix"` | Temporary maintenance AP password; minimum 8 characters |

On Waveshare profiles, a short BOOT press after normal startup requests a software reboot; holding the button for at least two seconds starts OTA maintenance. GPIO0 remains an ESP32 boot strap, so do not hold it low during reset/power-up unless deliberately entering the ROM boot mode.

## Waveshare media / PSRAM policy

These are advanced profile-level controls. The checked-in Waveshare values are hardware-qualified as a set.

| Macro | Waveshare value | Meaning |
| --- | ---: | --- |
| `IDOTMATRIX_MEMORY_TELEMETRY` | `1` | Structured `[MEM]` / `[LAT]` qualification telemetry |
| `IDOTMATRIX_GIF_PSRAM_STAGE_MAX_BYTES` | `2097152UL` | Maximum compressed GIF eligible for transient whole-file PSRAM staging |
| `IDOTMATRIX_GIF_PSRAM_RESERVE_BYTES` | `4194304UL` | PSRAM reserve that staging/cache policy must preserve |
| `IDOTMATRIX_GIF_PSRAM_CACHE_MAX_BYTES` | `1048576UL` | Total persistent compressed GIF cache budget |
| `IDOTMATRIX_GIF_PSRAM_CACHE_ENTRY_MAX_BYTES` | `524288UL` | Per-entry cache limit |
| `IDOTMATRIX_GIF_PSRAM_CACHE_MAX_ENTRIES` | `12` | Maximum cache entries |
| `IDOTMATRIX_CAROUSEL_GIF_PREFETCH` | `1` | One-item Carousel look-ahead prefetch |
| `IDOTMATRIX_CAROUSEL_GIF_PREFETCH_DELAY_MS` | `250UL` | Delay before starting look-ahead work |
| `IDOTMATRIX_CAROUSEL_GIF_PREFETCH_CHUNK_BYTES` | `4096U` | Bytes copied per prefetch loop iteration |

Generic source defaults disable staging/cache/prefetch by setting the size/enable controls to zero. LittleFS remains authoritative even when the PSRAM optimizations are enabled.

## BLE backend and NimBLE stack headroom

Waveshare profiles use:

```text
IDOTMATRIX_USE_NIMBLE=1
MYNEWT_VAL_NIMBLE_HOST_TASK_STACK_SIZE=8192
```

The 8 KiB host-task stack is retained as Waveshare headroom, but stack size is not used as the sole safety boundary. Persistent Carousel GIF/IMAGE/TEXT assets are buffered in PSRAM and filesystem publication is deferred to `loopTask`. Do not reduce the stack or change this ownership boundary without repeating the consecutive-Carousel stress test. Non-Waveshare environments intentionally do not inherit this override.

## Internal diagnostic switches

The main translation unit also contains narrow protocol/debug switches such as `PNG_DIAG_SERIAL`, `TEXT_PROTOCOL_DEBUG`, `BULK_PROTOCOL_DEBUG`, `GRAFFITI_PROTOCOL_DEBUG`, `PRESET_PROTOCOL_DEBUG`, `CAROUSEL_PROTOCOL_DEBUG` and `DEVICE_INFO_PROTOCOL_DEBUG`.

They are intentionally disabled in the release and are **not** treated as stable user-facing configuration API. Enable them only for targeted protocol investigation and do not publish a release with verbose tracing accidentally enabled.

## Recommended change policy

- Prefer an existing PlatformIO environment over inventing a new flag combination.
- Put wiring/peripheral overrides in `IDotMatrixUserConfig.h`.
- Treat media-memory limits, NimBLE stack size, display geometry and orientation thresholds as qualified profile parameters rather than casual preferences.
- After changing profile-level memory, BLE stack, display or timing controls, rerun the host regression suite, rebuild the target from clean state and repeat the relevant physical stress test.
