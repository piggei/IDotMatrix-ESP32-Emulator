# Waveshare Build 200 - Persistent Compressed GIF Source Cache

**Release:** `0.6.0-dev.3`  
**Build:** `200`  
**Target:** Waveshare ESP32-S3 RGB Matrix (`ESP32-S3-N32R16`)

## Purpose

Build 200 layers a bounded persistent PSRAM cache over the hardware-qualified B199 whole-file GIF staging path. The cache stores **complete compressed GIF source bytes only**. It does not cache decoded frames, render output, or AnimatedGIF decoder state.

The goal is to remove repeated LittleFS reads and repeated LittleFS-to-PSRAM copies when Carousel/Preset media is replayed. A cold miss still uses the B199 staging path; a warm hit is served directly from the cached compressed source.

## Policy

| Setting | B200 value |
| --- | ---: |
| Transient staging cap | 2 MiB per GIF |
| Persistent cache budget | 1 MiB total |
| Persistent cache entry cap | 512 KiB per GIF |
| Maximum cache entries | 12 |
| PSRAM reserve | 4 MiB |
| Cache identity | path + size + CRC32 |
| Eviction | LRU, active entry protected |

All three Waveshare logical profiles enable the same policy. Other profiles default both staging and persistent caching to disabled.

## Cold path

1. Validate the logical media identity and look for a cache hit.
2. On a miss, retain the B199 whole-file staging policy.
3. Copy the compressed GIF from LittleFS to PSRAM in 4 KiB chunks.
4. Open AnimatedGIF on the PSRAM source.
5. If the staged source is cache-eligible, transfer ownership of that same allocation into the cache. No second copy or allocation is performed.

A file larger than 512 KiB but not larger than the 2 MiB B199 staging cap can still use transient staging; it is simply not retained in the persistent cache.

## Warm path

A hit requires an exact `{path,size,CRC32}` match. The selected entry is pinned before AnimatedGIF opens it. Carousel/Preset known-GIF validation accepts the cached identity and therefore avoids a redundant complete-file CRC reread from LittleFS. AnimatedGIF read/seek callbacks then access the cached bytes directly.

The cache entry remains pinned until `AnimatedGIF::close()` and decoder destruction have completed. LRU eviction never selects the active entry.

## Invalidation

Cache entries are invalidated when a source path is replaced or cleared:

- live `GIF_PLAY_FILE` promotion;
- event GIF play-file replacement;
- Carousel slot clear/replacement;
- Preset slot clear/replacement.

A new file with the same path but a different size or CRC is therefore never allowed to reuse stale bytes.

## Fallback

LittleFS remains authoritative. If persistent caching is disabled, the source exceeds the cache entry cap, cache room cannot be created, or the source has no trustworthy CRC identity, B200 keeps the B199 transient staging/file fallback behavior unchanged.

The B199 2 MiB staging cap and 4 MiB PSRAM reserve remain the final guard before file-backed decoding.

## Telemetry

B200 adds machine-readable cache lines:

```text
[GIFCACHE] state=miss reason=identity ...
[GIFCACHE] state=insert reason=cold ...
[GIFCACHE] state=hit reason=identity ...
[GIFCACHE] state=evict reason=lru ...
[GIFCACHE] state=invalidate reason=path ...
[GIFCACHE] state=bypass reason=entry_cap ...
```

Each line includes current/peak cache bytes and cumulative hit/miss/insert/eviction/invalidation/bypass counters.

Additional memory/latency tags:

```text
[MEM] ... tag=gif.cache.insert ...
[MEM] ... tag=gif.cache.hit ...
[MEM] ... tag=gif.cache.evict ...
[MEM] ... tag=gif.cache.invalidate ...
[LAT] ... tag=gif.cache.hit_us us=...
```

B199 `[GIFSTAGE]`, `gif.stage.*`, decoder-open and first-frame telemetry remain enabled.

## B199 physical evidence inherited

Physical B199 testing on the Waveshare 64x64 profile confirmed:

- repeated Carousel GIF PSRAM staging and release without memory drift;
- live GIF staging from the official app;
- at least 93 successful staging attempts with zero observed staging fallback in the captured soak;
- peak tested compressed source about 176 KiB;
- no observed Guru Meditation or `nimble_host` stack-canary regression.

The deliberate >2 MiB LittleFS fallback was not physically exercised in that trace; it remains covered by the guarded implementation and host regression tests.

## B200 physical qualification

B200 was exercised on the real Waveshare 64x64 target before B201 work began. Field evidence confirmed:

- cold `miss -> GIFSTAGE -> insert`;
- warm `state=hit` with no repeated `gif.stage.copy_us` for retained identities;
- Preset and Carousel path invalidation followed by correct cold re-population;
- stable playback after invalidation and repeated cache hits;
- the 12-entry limit under pressure;
- real `state=evict reason=lru` events with the least-recently-used source removed and the slot immediately reused;
- subsequent cache hits remained correct after eviction;
- cache peak remained below the 1 MiB budget in the captured corpus and no cache-related crash was observed.

The deliberate >2 MiB B199 file fallback remains outside the captured physical evidence.

## Exit gate

Build 200 is hardware-qualified: cold media inserted correctly, warm media hit without re-copying from LittleFS, source replacement invalidated stale entries, active playback survived cache pressure, LRU eviction/reuse behaved correctly, memory remained bounded by policy, and no B198/B199 regression was observed.
