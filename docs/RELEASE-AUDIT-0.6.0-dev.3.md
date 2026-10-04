# 0.6.0-dev.3 / Build 201 Development Audit

**Release:** `0.6.0-dev.3`  
**Build:** `201`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B201`

## Scope

Build 201 adds one-item Carousel GIF prefetch only on the three Waveshare profiles. It is layered on the physically qualified B200 persistent cache and does not replace the B199/B200 cold path.

## Resource policy

- B199 transient stage max: 2 MiB;
- PSRAM reserve: 4 MiB;
- B200 persistent cache total: 1 MiB;
- persistent entry max: 512 KiB;
- maximum cache entries: 12;
- one prefetch work buffer at a time;
- prefetch candidate must fit the 512 KiB cache entry cap;
- copy quantum: 4 KiB per loop service iteration;
- start delay: 250 ms after active Carousel slot start.

The prefetch buffer is temporary and becomes part of the existing cache allocation only after validation. There is no second copy during insertion.

## Correctness guards

- only the immediate next Carousel slot is considered;
- TEXT next slots are skipped rather than scanned past;
- current-media rendering runs before prefetch service;
- complete byte count and CRC32 are validated before cache ownership transfer;
- target metadata is rechecked while copying and before insertion;
- partial work is cancelled on advance/stop/stale identity;
- current file-backed GIF fallback suppresses prefetch to avoid extra LittleFS contention;
- B200 active-entry pinning and LRU policy remain unchanged;
- failure/skip never prevents ordinary cold playback.

## Regression coverage

The dependency-free suite covers:

- Waveshare-only enablement and disabled default elsewhere;
- one-immediate-item policy;
- 250 ms delay and 4 KiB incremental copy;
- rendering-before-prefetch loop ordering;
- concurrent CRC accumulation and validation before cache insertion;
- PSRAM reserve/largest-block and cache-entry-cap guards;
- advance/stop/stale cancellation;
- machine-readable diagnostics;
- host C++ syntax compilation of the exact prefetch block with prefetch enabled and disabled;
- all inherited B197/B198/B199/B200 regression tests.

## Physical gate

B201 is not hardware-qualified until a real Waveshare run shows a next-GIF `scheduled -> begin -> cached` sequence followed by a warm cache hit at the actual Carousel transition without visible playback regression. GIF -> TEXT -> GIF skip/schedule behavior and stale-target cancellation should also be exercised.
