# Waveshare Build 201 - One-Item Carousel GIF Prefetch

**Release:** `0.6.0-dev.3`  
**Build:** `201`  
**Target:** Waveshare ESP32-S3 RGB Matrix (`ESP32-S3-N32R16`)

## Purpose

Build 201 layers a conservative one-item Carousel look-ahead prefetch over the hardware-qualified B200 compressed-GIF PSRAM cache. The prefetch stores only the complete compressed source for the **immediate next Carousel item when that item is a GIF**. It does not decode frames ahead of time and it does not scan past a TEXT item.

The goal is to turn the next Carousel transition into a normal B200 cache hit without introducing a background task or a long synchronous LittleFS copy that could stall animation rendering.

## Inherited qualified policy

B201 keeps the B199/B200 resource policy unchanged:

| Setting | Value |
| --- | ---: |
| Transient GIF stage cap | 2 MiB |
| Persistent cache budget | 1 MiB |
| Persistent entry cap | 512 KiB |
| Maximum cache entries | 12 |
| PSRAM reserve | 4 MiB |
| Cache identity | path + size + CRC32 |
| Cache eviction | LRU, active decoder entry protected |

B200 hardware testing qualified cold insert, warm hit, Preset/Carousel invalidation, cache re-population, 12-entry pressure and real LRU eviction/reuse on the Waveshare 64x64 target.

## Prefetch policy

- Enabled only on the three Waveshare profiles.
- The immediate next Carousel slot is the only candidate.
- A TEXT next slot is deliberately skipped; when that TEXT slot becomes active, its own immediate successor can be considered.
- GIFs above the 512 KiB persistent-cache entry limit are skipped.
- Already-cached identities are skipped without touching playback hit/miss counters.
- Prefetch begins 250 ms after the current Carousel slot starts.
- At most 4096 bytes are read per `loop()` service iteration.
- The active media update runs before the prefetch service in the loop, so current rendering has priority.
- Prefetch is suppressed when the current Carousel GIF had to fall all the way back to file-backed AnimatedGIF callbacks, avoiding extra concurrent LittleFS traffic in the degraded path.

## Integrity and ownership

The prefetch buffer is separate from the active B199 transient stage and B200 cache entry. The file is read incrementally while CRC32 is accumulated over the copied bytes.

The buffer becomes a B200 cache entry only after:

1. the declared byte count has been copied;
2. the computed CRC matches the Carousel metadata CRC;
3. the target slot still has the same GIF identity;
4. cache budget/slot policy can accept the entry.

Ownership is then transferred directly to the cache with no second source copy. If any check fails, the partial/complete prefetch buffer is discarded and ordinary B200 cold playback remains available.

## Cancellation

An in-progress prefetch is cancelled when:

- Carousel playback stops;
- playback advances before the copy has completed;
- the target slot metadata changes during the copy;
- the target becomes invalid for the active Carousel context.

This makes prefetch opportunistic only. It is never required for correctness.

## Telemetry

Build 201 adds:

```text
[GIFPREFETCH] state=scheduled reason=next ...
[GIFPREFETCH] state=begin reason=copy ...
[GIFPREFETCH] state=cached reason=ok ...
[GIFPREFETCH] state=skip reason=cached|next_text|entry_cap|reserve|... ...
[GIFPREFETCH] state=fail reason=crc|source|read|cache|... ...
[GIFPREFETCH] state=cancel reason=advance|stop|stale|... ...
```

Each line includes target slot, byte count, copied bytes and cumulative attempt/success/skip/failure/cancel counters.

Additional measurements:

```text
[MEM] ... tag=gif.prefetch.before ...
[MEM] ... tag=gif.prefetch.begin ...
[MEM] ... tag=gif.prefetch.cached ...
[MEM] ... tag=gif.prefetch.fail ...
[MEM] ... tag=gif.prefetch.cancel ...
[LAT] ... tag=gif.prefetch.total_us us=...
```

B199 `[GIFSTAGE]` and B200 `[GIFCACHE]` telemetry remain unchanged.

## Physical test plan

1. Build/upload `waveshare_s3_rgbmatrix_64x64` and confirm `IDOTMATRIX_FW=0.6.0-dev.3-B201`.
2. Start with a cold cache and run a Carousel containing consecutive GIF slots.
3. During slot N, expect `GIFPREFETCH scheduled -> begin -> cached` for GIF slot N+1.
4. At the N -> N+1 transition, expect `GIFCACHE state=hit` for the prefetched identity and no `gif.stage.copy_us` for that transition.
5. Confirm current GIF animation remains visually smooth while the incremental copy is running.
6. Exercise a GIF -> TEXT -> GIF sequence. The first transition should report a `next_text` skip; while TEXT is active, the following GIF should become eligible for prefetch.
7. Replace a prefetched Carousel slot before/while it is being used and confirm stale data is cancelled/invalidated and never displayed.
8. Exercise enough distinct GIFs to retain B200 LRU pressure behavior and verify active playback remains protected.
9. Confirm no B198 deferred-commit, B199 staging or B200 cache regression.

## Exit gate

B201 passes when the immediate next GIF is populated ahead of the transition, the transition becomes a normal cache hit without a repeated cold copy, partial/stale work is safely cancelled, playback remains visually stable, and the B200 bounded-cache policy remains intact.
