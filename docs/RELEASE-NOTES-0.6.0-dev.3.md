# iDotMatrix ESP32 Emulator 0.6.0-dev.3

**Release:** `0.6.0-dev.3`  
**Build:** `199`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B199`

## Purpose

Build 199 resumes the Waveshare memory/media roadmap after B198 physically qualified the BLE/filesystem deferred-commit hardening. B199 adds **transient whole-file GIF source staging in PSRAM** only; it does not yet introduce a persistent cache or Carousel prefetch.

The B196/B198 physical envelope showed roughly 15.05 MiB (15.78 MB decimal) PSRAM still free under representative 64x64 GIF/Carousel/Preset load, while internal/DMA memory is much scarcer. B199 therefore uses PSRAM deliberately for the active compressed GIF source while preserving strict fallback to the B198 LittleFS path.

## Inherited hardware-qualified baseline

- B194 OTA upload, interrupted-upload recovery and persistence: PASS.
- B195 logical 64x64, 32x32->64x64 and 16x16->64x64 scaling: PASS.
- B198 real PlatformIO build/upload: PASS.
- Repeated Preset TEXT/font deferred commits: PASS with no `nimble_host` stack-canary reboot.
- Repeated Carousel deferred commits and subsequent playback: PASS.

## B199 changes

- `FW_BUILD` incremented to 199.
- Waveshare profiles enable a 2 MiB transient GIF staging cap and preserve a 4 MiB PSRAM reserve.
- Staging requires the requested source to fit the cap, current free PSRAM, reserve policy and current largest contiguous PSRAM block.
- The complete compressed source is copied from LittleFS into `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` memory in 4 KiB chunks with `yield()`.
- Existing AnimatedGIF callbacks serve the staged PSRAM source when active; otherwise they open/read/seek the original LittleFS path.
- The staged source is released only after decoder close/teardown.
- `[GIFSTAGE]` diagnostics report state/reason/bytes/peak/attempts/successes/fallbacks/cap/reserve.
- `[MEM] gif.stage.*` and `[LAT] gif.stage.copy_us` records extend the B196 telemetry.
- `gif.*.first_frame_us` includes staging time; decoder `gif.*.open_us` remains measured separately.

## Explicit non-changes

B199 does **not** add persistent GIF cache entries, LRU eviction, Carousel prefetch, decoded-frame caching, new filesystem transactions, BLE protocol changes, OTA changes, scaling changes, HUB75 changes or a NimBLE stack-size override. Non-Waveshare profiles default GIF staging to disabled.

## Validation

Run:

```bash
python3 tests/run_tests.py
pio run -e waveshare_s3_rgbmatrix_64x64
```

Then upload B199 and exercise live GIF, mixed GIF/TEXT Carousel, Preset GIF and Alarm/Schedule GIF. Confirm `[GIFSTAGE] state=psram reason=ok` for eligible files, normal playback, PSRAM recovery when playback stops/switches, and transparent file-backed playback for an intentionally over-cap GIF or an artificially constrained staging build.
