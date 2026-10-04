# iDotMatrix ESP32 Emulator 0.6.0-dev.3

**Release:** `0.6.0-dev.3`  
**Build:** `201`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B201`

## Purpose

Build 201 adds a conservative **one-item Carousel GIF look-ahead prefetch** on top of the hardware-qualified B200 persistent compressed-source cache. The prefetch is opportunistic only: ordinary B200 cache miss/staging/file behavior remains authoritative and unchanged.

## Inherited hardware-qualified baseline

- B194 OTA upload, interrupted-upload recovery and persistence: PASS.
- B195 logical 64x64, 32x32->64x64 and 16x16->64x64 scaling: PASS.
- B198 Preset/Carousel deferred filesystem commit hardening: PASS.
- B199 Carousel/live whole-GIF PSRAM staging/release: PASS.
- B200 cold insert, warm hit, Preset/Carousel invalidation, re-population, 12-entry pressure and real LRU eviction/reuse: PASS.

## B201 changes

- `FW_BUILD` incremented to 201.
- Enables `IDOTMATRIX_CAROUSEL_GIF_PREFETCH=1` only on the three Waveshare profiles.
- Schedules only the immediate next Carousel slot and only when it is a cache-eligible GIF.
- Waits 250 ms after the current slot starts, then reads at most 4 KiB per loop iteration after active GIF/TEXT rendering.
- Computes CRC32 while copying and inserts into the existing B200 LRU cache only after exact byte-count/CRC validation.
- Uses a separate PSRAM prefetch buffer, so the active B199 transient stage/cache source lifetime is unchanged.
- Cancels partial work on stop, advance or target metadata changes.
- Suppresses prefetch while current Carousel GIF playback is using the file-backed fallback path.
- Adds `[GIFPREFETCH]` diagnostics plus `gif.prefetch.*` memory/latency telemetry.

## Explicit non-changes

B201 does **not** add decoded-frame caching, a new FreeRTOS/background worker, multiple-item look-ahead, scanning past TEXT slots, new BLE protocol behavior, OTA changes, scaling changes, HUB75 changes, NimBLE stack changes or new board peripherals. The B199 stage and B200 cache budgets remain unchanged.

## Validation

Run:

```bash
python3 tests/run_tests.py
pio run -e waveshare_s3_rgbmatrix_64x64
```

Then run a cold mixed Carousel. For consecutive GIF slots, expect `GIFPREFETCH scheduled -> begin -> cached` while slot N is playing, followed by a normal `GIFCACHE state=hit` for slot N+1 with no repeated `gif.stage.copy_us` at the transition. Also exercise GIF -> TEXT -> GIF, target replacement/cancellation and continued B200 LRU behavior.
