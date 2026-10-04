# Waveshare ESP32-S3 RGB Matrix - Build 196 Memory Baseline

**Release:** `0.6.0-dev.3`  
**Build:** `196`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B196`

## Purpose

Build 196 is measurement-only. It inherits the hardware-qualified B195 scaling runtime and adds structured memory/latency telemetry to the three Waveshare profiles. No allocator, framebuffer placement, GIF source path, cache policy, Carousel behavior, storage transaction or protocol behavior is intentionally changed.

Build 195 was field-confirmed working at logical 16x16, 32x32 and native 64x64 on the same physical 64x64 Waveshare HUB75 panel. Build 194 already field-confirmed complete OTA upload, interrupted-upload recovery to the previous firmware and preserved persistent state. These are the behavior baselines B196 must preserve.

## Scope

The following build flag is enabled only in the three Waveshare environments:

```text
IDOTMATRIX_MEMORY_TELEMETRY=1
```

MatrixPortal, ESP32-C3 and classic ESP32 profiles compile the telemetry API to no-op inline calls and do not receive the B196 serial instrumentation.

## Structured memory line

B196 emits machine-readable snapshots:

```text
[MEM] t_ms=1234 tag=setup.after_hub75 int_free=... int_min=... int_largest=... dma_free=... dma_min=... dma_largest=... psram_total=... psram_free=... psram_min=... psram_largest=...
```

Fields:

| Field | Meaning |
| --- | --- |
| `int_free` | current internal 8-bit-capable DRAM bytes |
| `int_min` | low-water internal 8-bit-capable DRAM bytes since boot |
| `int_largest` | largest current contiguous internal 8-bit block |
| `dma_free` | current internal DMA-capable bytes |
| `dma_min` | low-water internal DMA-capable bytes since boot |
| `dma_largest` | largest current contiguous DMA-capable block |
| `psram_total` | configured PSRAM bytes |
| `psram_free` | current external PSRAM bytes available to the heap |
| `psram_min` | low-water external PSRAM bytes since boot |
| `psram_largest` | largest current contiguous PSRAM block |

The low-water values are cumulative since boot. Event-to-event deltas should therefore be interpreted together with the current free/largest-block fields.

## Structured latency line

Measured operations emit:

```text
[LAT] t_ms=2345 tag=gif.carousel.first_frame_us us=12345
```

Latency is reported in microseconds. The instrumentation is diagnostic and adds serial-output overhead, so B196 values are intended for relative engineering decisions rather than as a production performance benchmark.

## Measurement points

Boot/runtime:

- `setup.begin`
- `setup.before_logical_buffers`
- `setup.after_logical_buffers`
- `setup.after_storage`
- `setup.before_hub75`
- `setup.after_hub75`
- `setup.after_ble`
- `setup.after_ota_arm`
- `setup complete`
- `runtime.periodic` every 30 seconds

Media/runtime:

- live and Carousel TEXT before parse / after render, plus parse+render latency;
- live/event/Carousel GIF before open / after open / first visible frame, plus open and first-frame latency;
- Carousel slot start snapshots and start latency;
- Schedule PNG before/after decode and decode latency;
- RAW RGB before allocation / after allocation / after free;
- full-raster Graffiti before allocation / after allocation / after free.

OTA:

- before starting Wi-Fi AP;
- after captive-portal AP/server start;
- before and after `Update.begin()`;
- complete, failed or aborted upload, including total upload latency.

## Field workload

Use `waveshare_s3_rgbmatrix_64x64` as the primary memory envelope because it has the largest logical framebuffers. The 16x16 and 32x32 environments may then be measured for comparison.

Recommended primary run:

1. cold boot and capture the complete setup sequence;
2. leave the device idle for at least 60 seconds;
3. connect/disconnect/reconnect BLE once;
4. send representative TEXT, including a large 32x64-glyph payload if available;
5. send a full RAW/static frame;
6. exercise full-raster Graffiti;
7. play a live GIF from a cold open at least three times;
8. run a mixed GIF/TEXT Carousel through several complete transitions;
9. exercise Schedule TEXT, GIF and PNG content;
10. enter OTA maintenance mode and capture the AP-start snapshots;
11. perform one complete OTA upload and, separately, one interrupted upload if desired for memory comparison.

No B196 result should be used to choose PSRAM limits until the real 64x64 workload has been captured.

## Log summary helper

Save the serial output to a text file, then run:

```bash
python3 tools/summarize_memory_telemetry.py serial.log
```

The dependency-free helper reports per-tag minimum free/largest-block observations, latency min/average/max and global low-water observations.

## Exit gate

B196 is complete when:

- the 64x64 Waveshare build compiles and boots with the B195 behavior intact;
- representative BLE/display/media paths remain functional;
- a serial log captures the boot and media/OTA points above;
- internal DRAM, DMA heap and PSRAM low-water/largest-block values are available under real Carousel workload;
- GIF first-frame and Carousel transition/start latency data are available;
- the measurements are sufficient to guide a later guarded transient GIF-staging reserve/cap without guessing, after the B197 BLE/filesystem hardening gate closes.

B196 did not move buffers into PSRAM or introduce GIF staging/cache/prefetch. The physical trace was reviewed and PSRAM optimization is intentionally delayed until the B197 BLE/filesystem hardening gate passes.

## Physical field result

A real 64x64 Waveshare capture was completed far enough to establish the first memory/GIF envelope and to expose a correctness issue before PSRAM staging should proceed. The dependency-free summarizer reported:

```text
int_free low observation: 175660
int_min cumulative low-water: 169536
internal largest block minimum: 131072
dma_free low observation: 167888
dma_min cumulative low-water: 161764
dma largest block minimum: 131072
psram_free low observation: 15794436
psram largest block minimum: 15728640
```

The logical-buffer setup delta was approximately 36.9 KiB in PSRAM with effectively unchanged internal free memory, showing that the current ESP32 heap policy already places those large ordinary allocations in PSRAM on this Waveshare configuration.

Captured GIF timing included Carousel opens around 6.2-8.7 ms, normal Carousel first frames around 16.5-18.7 ms (with a slower boot-path sample) and live first frames around 14.8-18.0 ms.

### Blocking failure discovered

During upload of a Preset containing TEXT/font data, the device rebooted with:

```text
Stack canary watchpoint triggered (nimble_host)
```

The decoded backtrace entered `commitPresetSlot()` -> `LittleFS.remove()` -> LittleFS/VFS -> flash write directly from the FA02 NimBLE callback. Because substantial internal/DMA/PSRAM memory remained, the failure is treated as a NimBLE task-stack/context problem rather than general heap exhaustion.

The originally planned GIF staging build is therefore postponed. Build 197 first hardens Preset/Carousel final filesystem publication by moving it to `loopTask`. See [`WAVESHARE-B197-BLE-FS-HARDENING.md`](WAVESHARE-B197-BLE-FS-HARDENING.md).
