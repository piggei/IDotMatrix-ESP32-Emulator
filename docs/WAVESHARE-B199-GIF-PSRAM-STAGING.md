# Waveshare Build 199 - Transient Whole-GIF PSRAM Staging

**Release:** `0.6.0-dev.3`  
**Build:** `199`  
**Primary target:** Waveshare ESP32-S3 RGB Matrix, logical/physical 64x64

## Goal

Reduce repeated LittleFS read/seek traffic during active GIF decoding by staging one complete **compressed GIF source** in external PSRAM. Preserve correctness by keeping LittleFS authoritative and falling back to the B198 file-backed callback path whenever staging is unsafe or unavailable.

## Initial policy

| Setting | B199 value |
| --- | ---: |
| Maximum staged GIF source | 2 MiB |
| Required PSRAM reserve after allocation | 4 MiB |
| Staged objects | one active source only |
| Allocation caps | `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` |
| Copy chunk | 4 KiB with `yield()` |
| Persistent cache | none |
| Prefetch | none |

The same flags are applied to Waveshare logical 16x16, 32x32 and 64x64 profiles. Other profiles default staging to disabled.

## Guards and fallback reasons

`[GIFSTAGE]` reports `state=psram reason=ok` on success or `state=fs` with one of:

- `invalid` - missing source/size/runtime state;
- `cap` - source larger than 2 MiB;
- `largest` - current largest contiguous PSRAM block is too small;
- `reserve` - allocation would violate the 4 MiB reserve;
- `source` - source file is missing or size differs;
- `alloc` - PSRAM allocation failed;
- `copy` - complete source copy failed.

Every fallback continues through the same LittleFS decoder callbacks used by B198.

## Lifetime

The active staged buffer belongs to the active GIF decoder. It is not cached after playback. Decoder close occurs first; the PSRAM source is freed only after `AnimatedGIF::close()` and object teardown so source callbacks can never observe freed memory.

## Telemetry

Example success:

```text
[GIFSTAGE] state=psram reason=ok bytes=123456 requested=123456 peak=123456 attempts=1 ok=1 fallback=0 max=2097152 reserve=4194304
[LAT] ... tag=gif.stage.copy_us us=...
[MEM] ... tag=gif.stage.psram ...
```

`gif.stage.released` records the post-free memory state when an active staged source is torn down. `gif.*.first_frame_us` measures the complete cold-start interval including staging. `gif.*.open_us` remains decoder-open time only.

## Physical test plan

1. Build/upload `waveshare_s3_rgbmatrix_64x64`.
2. Capture cold boot and at least 60 s idle memory telemetry.
3. Send/play a normal live GIF and confirm `state=psram`.
4. Replay/switch several GIFs and verify PSRAM free returns after teardown before the next allocation.
5. Run a mixed GIF/TEXT Carousel for several complete cycles.
6. Exercise a Preset containing GIF and an Alarm/Schedule GIF if available.
7. Verify B198 Preset/Carousel commit stability remains intact.
8. Exercise fallback: use a GIF larger than 2 MiB if protocol/storage limits permit, or temporarily build with a smaller staging cap/reserve for qualification; playback must remain file-backed and correct.
9. Save `[GIFSTAGE]`, `[MEM]` and `[LAT]` data for comparison with B196/B198.

## Exit gate

B199 passes when staging succeeds on eligible GIFs without playback regressions or memory drift, forced fallback remains functional, and no BLE/filesystem crash returns. Only then should Build 200 introduce persistent source caching.
