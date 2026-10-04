# 0.6.0 Final Release Audit

**Release:** `0.6.0`  
**Build:** `213`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-B213`

## Scope

This audit finalizes the qualified 0.6.0 development line as the stable 0.6.0 release. Build 213 is a promotion/cleanup build over hardware-qualified Build 212: runtime semantics are intentionally unchanged.

## Release-critical findings closed

- Static Device Assets compatibility: app-observed type-2 PNG is supported and physically rendered in Carousel.
- Mixed Carousel ownership: PNG/GIF/TEXT remain inside Carousel ownership and TEXT advances normally to the next slot.
- Inter-asset timing: the settle window is 8 seconds, covering the observed ~3.8-second valid app pause.
- BLE-host stack exhaustion: Waveshare uses an 8 KiB NimBLE host-task stack; repeated real Carousel replacement completed without the prior `nimble_host` stack-canary.
- Filesystem work in latency-sensitive receive paths: final Carousel/Preset publication and static type-2 LittleFS work execute on `loopTask`.
- Carousel replacement safety: journal/rollback protects the previous complete bank across interrupted replacement.
- Preset replacement safety: candidate staging keeps the previous complete live-session Preset authoritative until activation succeeds.

## Documentation cleanup

- Public identity is `0.6.0 / Build 213`.
- Development release notes/audits and per-build Waveshare qualification notes are removed from the stable package; their chronology remains summarized in `HISTORY.md`.
- `README.md`, hardware support, PlatformIO, OTA, protocol comparison and future-work documents are aligned to the stable state.
- Stale 3-second Carousel-settle wording is removed; the qualified value is 8 seconds.
- The obsolete RAW-only/transient-index interpretation of static Device Assets is removed from current-facing documentation and code comments.

## Code cleanup

- Development-build chronology comments in runtime code are replaced with behavior-oriented comments where practical.
- Diagnostic switches remain disabled by default.
- No qualified runtime path is intentionally redesigned in the stable promotion.

## Verification requirements/results

- dependency-free host regression suite: **135/135 PASS**;
- shell syntax for `update_idotmatrix_emulator.sh`: **PASS**;
- stable release/build identity scan: **PASS**;
- relative Markdown link scan: **55 local targets checked, 0 broken**;
- package hygiene before archive creation: **PASS**; no `.pio`, Python cache, local `src/IDotMatrixUserConfig.h` or generated firmware artifacts;
- fresh all-environment PlatformIO compile: not available in the packaging environment and therefore not claimed.

## Final assessment

Given the recorded hardware qualification of Build 212 and the stable promotion policy above, Build 213 is suitable as the 0.6.0 stable source package once the final-tree checks listed in this audit pass.
