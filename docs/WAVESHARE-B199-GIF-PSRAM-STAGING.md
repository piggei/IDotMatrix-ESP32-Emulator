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

The B199 hardware gate was closed for the eligible PSRAM staging path after Carousel/live playback and repeated soak showed correct staging/release with no crash or memory drift. The deliberate >2 MiB fallback was not field-exercised in the captured run; its guards remained host-tested. Build 200 therefore proceeds with persistent source caching while preserving the same fallback implementation.

## Physical qualification result

B199 was exercised on the real Waveshare 64x64 target before B200 work began.

- Repeated Carousel GIF staging/release completed correctly with PSRAM returning after each source switch.
- A live GIF from the official app also staged and played correctly.
- The captured soak reached at least 93 successful staging attempts with zero observed fallback.
- The largest captured compressed GIF was about 176 KiB.
- No Guru Meditation or `nimble_host` stack-canary regression was observed.

The deliberate >2 MiB LittleFS fallback was not physically exercised in this capture. Therefore the PSRAM staging path is hardware-qualified; the forced-cap fallback remains host/regression covered rather than field-qualified.
