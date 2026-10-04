# Release Audit - iDotMatrix ESP32 Emulator 0.6.0

## Scope

This audit covers the updated final `0.6.0` source package. The qualified media/storage runtime is preserved, with one narrow display-ownership correction: starting a Device Assets bank replacement now establishes Carousel view intent so a successfully committed bank can replace an active Preset/Default on screen.

## Release identity

- Public release: `0.6.0`
- Internal diagnostic build: `223`
- Firmware signature: `IDOTMATRIX_FW=0.6.0`

The internal build number is deliberately not part of the public firmware signature.

## Code review findings

- The final static review identified and corrected one display-ownership bug: a Carousel bank could commit successfully while active Preset/Default playback remained on screen because bank setup inherited a false view-intent state.
- Protocol debug switches remain disabled by default.
- Persistent Waveshare Carousel GIF/IMAGE/TEXT receive remains PSRAM-buffered; LittleFS publication occurs on `loopTask`, not in the NimBLE receive callback.
- Static Device Assets type `0x02` retains RGB24/PNG handling on the loop-side playback path.
- Device Assets bank setup now establishes Carousel view intent, and deferred setup preserves any later explicit Assets-view request; after commit/settle the new Carousel takes display ownership from an active Preset/Default.
- Transactional Carousel and Preset publication/recovery logic remains intact.
- The updater's post-upload ELF signature check was corrected to match the actual embedded signature (`IDOTMATRIX_FW=<release>`). The previous helper expected an obsolete `-B<build>` suffix even though the firmware did not embed one; the check was non-blocking but inconsistent.

## Hardware evidence

Physical Waveshare qualification already includes:

- native 64x64 HUB75 bring-up;
- logical 16x16, 32x32 and 64x64 scaling;
- OTA maintenance and interrupted-upload recovery;
- GIF staging/cache/prefetch;
- crash-recoverable Carousel bank replacement;
- Preset disconnect rollback/resume;
- static PNG Device Assets and mixed PNG/GIF/TEXT playback;
- three consecutive complete Carousel replacements with commit/playback after each and no `nimble_host` stack-canary, Guru Meditation or spontaneous reboot;
- BOOT short reboot / long OTA behavior;
- ES8311/I2S synthesized notification audio.

## Automated validation

Final pre-package validation produced **153/153 host regression tests passing**, `bash -n update_idotmatrix_emulator.sh` PASS, **55 source Markdown local links / 0 broken**, and **105 Wiki links / 0 broken**.

The final package must pass:

```bash
python3 tests/run_tests.py
bash -n update_idotmatrix_emulator.sh
```

Packaging hygiene requires no `.pio`, `.pytest_cache`, `__pycache__`, `.pyc`, generated firmware, map/ELF output or local `src/IDotMatrixUserConfig.h`.

## Tooling limitation of this audit environment

PlatformIO is not available in the assistant execution environment, so the complete embedded compile matrix cannot be re-run here. The repository retains the exact commands in `docs/RELEASE-VALIDATION.md`; physical Waveshare evidence was supplied from the release workstation.

## Remaining final gate

The updated ownership fix is covered by host regression but has not been physically re-qualified in this audit environment. Before tagging/re-publishing `v0.6.0`, start from active Preset/Default playback, upload a Carousel, wait for `[CARBANK] state=commit` plus the settle boundary, and confirm that the first valid Carousel slot takes display ownership automatically.

## Conclusion

The source tree is coherent as the updated `0.6.0` package, subject to the release workstation's normal PlatformIO build gate and the explicit Preset/Default-to-Carousel physical switch test above. No intermediate development-build chronology is required in the public release package; the engineering lessons are documented by problem and solution instead.
