# Waveshare ESP32-S3 RGB Matrix - Build 197 BLE / Filesystem Commit Hardening

> **Superseded before hardware qualification:** the first real PlatformIO build of B197 failed because Arduino auto-generated a prototype using `DeferredAssetCommitKind` before the enum declaration. Build 198 preserves this runtime design and corrects the helper signature. See [`WAVESHARE-B198-COMPILE-CORRECTION.md`](WAVESHARE-B198-COMPILE-CORRECTION.md).


**Release:** `0.6.0-dev.3`  
**Build:** `197`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B197`

## Purpose

Build 197 is a targeted correctness/hardening build triggered by a physical B196 failure while uploading a Preset containing TEXT/font data. It deliberately postpones the planned GIF PSRAM staging work until the BLE/filesystem crash is removed.

## B196 field failure

The physical Waveshare log recorded:

```text
Guru Meditation Error: Core 0 panic'ed (Unhandled debug exception)
Debug exception reason: Stack canary watchpoint triggered (nimble_host)
```

The decoded backtrace was not an out-of-memory failure. It showed the NimBLE FA02 write callback executing the following deep stack:

```text
FA02Callbacks::onWrite
  -> processFA02Write
  -> processFA02Packet
  -> processBulkPacket
  -> commitPresetSlot
  -> LittleFS.remove
  -> VFS / littlefs
  -> esp_partition_write / SPI flash
```

The crash occurred while `nimble_host` was performing LittleFS directory/flash work. Immediately before the failure, the B196 telemetry still showed approximately 175 KiB current internal free memory, 168 KiB DMA-capable free memory, a 131 KiB largest internal/DMA block and about 15.79 MB free PSRAM. The observed failure is therefore treated as task-stack exhaustion caused by deep filesystem/flash work on the NimBLE host task, not as general heap or PSRAM exhaustion.

## B196 memory observations retained

The captured B196 log also established useful baseline facts:

- global observed internal low-water: `169536` bytes;
- global observed DMA-capable low-water: `161764` bytes;
- global observed PSRAM low-water: `15794436` bytes;
- minimum observed internal/DMA largest block: `131072` bytes;
- minimum observed PSRAM largest block: `15728640` bytes;
- logical 64x64 buffers reduce PSRAM by approximately 36.9 KiB while internal free memory remains effectively unchanged, confirming that the current ESP32 heap policy already places those large ordinary allocations in PSRAM on this target;
- Carousel GIF open latency in the captured sample was about 6.2-8.7 ms;
- Carousel GIF first-frame latency was normally about 16.5-18.7 ms, with the boot-path sample higher;
- live GIF first-frame latency was about 14.8-18.0 ms.

These values are evidence from the captured workload only; they do not yet replace the complete B196 workload matrix.

## B197 runtime change

B197 keeps chunk reception and staging behavior unchanged but introduces a strict final-publication boundary for Preset and Carousel assets.

On receipt of the final bulk chunk, the BLE callback now:

1. verifies the received-size and CRC condition already available to the bulk layer;
2. records a small `DeferredAssetCommitState` descriptor;
3. returns without calling `flush()`, `close()`, `LittleFS.remove()`, `LittleFS.rename()`, Preset/Carousel metadata persistence or the final transfer ACK.

At the beginning of the next Arduino `loop()` iteration, while holding the existing runtime-state mutex, `processDeferredAssetCommit()`:

1. flushes and closes the staged file;
2. runs `commitPresetSlot()` or `commitCarouselSlot()`;
3. performs failure cleanup on the staged file when needed;
4. updates Carousel completion state when applicable;
5. resets the completed bulk transaction;
6. sends the final transfer ACK only after the transaction state is clean.

This preserves transfer ordering: the official app does not receive the completion ACK until the filesystem publication step has actually finished.

## Disconnect behavior

If BLE disconnects after the complete Preset/Carousel object has been received and its deferred commit is pending, B197 preserves the staged transaction and allows `loopTask` to finish publication. The final notification naturally becomes a no-op while disconnected. Partial transfers retain the existing abort/cleanup behavior.

## Explicit non-changes

Build 197 does not:

- increase the NimBLE host task stack as the primary fix;
- change NimBLE-Arduino version or MTU;
- change Preset/Carousel wire format, CRC rules, slot numbering or playback semantics;
- move chunk writes away from LittleFS;
- introduce whole-file GIF PSRAM staging, source cache or prefetch;
- change OTA, partitioning, HUB75/scaling or peripheral support;
- remove the B196 memory telemetry.

Chunk writes still occur through the existing receive path. B197 specifically removes the deepest final `flush/close/remove/rename/metadata` publication chain from `nimble_host`. If a future physical trace shows stack pressure during ordinary chunk writes or abort cleanup, those paths should be deferred in a subsequent isolated hardening build rather than hidden by a large task-stack increase.

## Physical qualification gate

Primary reproduction test:

1. boot `waveshare_s3_rgbmatrix_64x64` B197 with serial monitoring enabled;
2. connect the official iDotMatrix app;
3. repeatedly upload the same Preset with TEXT/font data that triggered the B196 crash;
4. activate and replay that Preset;
5. repeat with a mixed TEXT/GIF Preset;
6. upload/replace a Carousel repeatedly, including TEXT and GIF slots;
7. disconnect/reconnect BLE and repeat;
8. leave the resulting Preset/Carousel running for several minutes.

PASS requires:

- no `nimble_host` stack-canary panic;
- no reboot during Preset/Carousel finalization;
- uploaded media commits and plays correctly;
- subsequent bulk transfers continue normally;
- Carousel persistent state remains valid across reboot;
- Preset remains intentionally volatile as before;
- no regression in B194 OTA or B195 16/32/64 scaling behavior.

## Next step

B197 itself did not reach hardware because of the Arduino auto-prototype compile error corrected by B198. The same deferred-commit runtime was then physically qualified in B198 for both Preset and Carousel. PSRAM optimization resumes in B199 with guarded whole-file GIF source staging and LittleFS fallback, using the B196/B198 measurements as the starting envelope.
