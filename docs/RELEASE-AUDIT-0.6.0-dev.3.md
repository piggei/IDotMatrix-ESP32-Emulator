# 0.6.0-dev.3 / Build 199 Development Audit

**Release:** `0.6.0-dev.3`  
**Build:** `199`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B199`

## Scope

Build 199 is the first post-measurement PSRAM optimization for the standalone Waveshare emulator. It stages only the **currently active complete compressed GIF source** in PSRAM. LittleFS remains authoritative and the existing file-backed decoder path remains the correctness fallback.

## Baseline evidence used

The B196/B198 physical traces showed a cumulative internal-memory low-water around 165-170 KiB, DMA low-water around 157-162 KiB and PSRAM low-water still around 15.05 MiB (15.78 MB decimal) under representative 64x64 workloads. B198 also physically qualified the loop-side Preset/Carousel commit hardening that removed the B196 `nimble_host` stack-canary failure.

Those measurements justify a deliberately conservative initial policy rather than copying arbitrary limits: B199 caps a staged source at 2 MiB and refuses staging if doing so would leave less than 4 MiB PSRAM free or if no contiguous PSRAM block is large enough.

## Correctness model

- One active decoder, one optional transient staged source.
- Source bytes are copied from the authoritative LittleFS file before decoder open.
- AnimatedGIF uses one callback interface for both PSRAM and file-backed reads/seeks.
- Any cap/reserve/largest-block/source/allocation/copy failure falls back to LittleFS.
- Staging success is not required for playback correctness.
- The staged buffer outlives the decoder handle and is released only after `gif->close()`/decoder destruction.
- No cache survives a GIF switch; no source is retained for future reuse.

## Instrumentation

B199 adds machine-readable `[GIFSTAGE]` lines plus `gif.stage.before`, `gif.stage.psram`, `gif.stage.fallback` and `gif.stage.copy_us`. Existing GIF open/first-frame and global `[MEM]` telemetry remain enabled on Waveshare profiles.

## Host validation

The dependency-free suite includes B199 policy/callback/lifetime guards and a host C++ syntax stub that compiles the exact staging/callback source fragment with staging both enabled and disabled.

## Physical exit gate

1. Real `waveshare_s3_rgbmatrix_64x64` PlatformIO compile succeeds below the 3 MiB OTA slot limit.
2. Eligible live/Carousel/Preset/event GIFs report PSRAM staging and play correctly.
3. Repeated GIF switches free/reallocate PSRAM without downward drift.
4. A forced/real fallback case remains fully functional through LittleFS.
5. Preset/Carousel deferred commit hardening remains stable; no `nimble_host` canary/reboot.
6. Compare `gif.stage.copy_us`, decoder open and total first-frame latency with B196/B198 before selecting persistent-cache limits.

Persistent GIF source caching is intentionally deferred to Build 200.
