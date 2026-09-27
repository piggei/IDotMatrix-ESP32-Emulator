# 0.5.2 Final Release Audit

**Release:** `0.5.2`  
**Build:** `190`  
**Firmware signature:** `IDOTMATRIX_FW=0.5.2-B190`

## Scope

This audit reviews the stable 0.5.2 Build 190 package. Build 190 supersedes Build 189 because the repository update helper previously assumed pytest was installed in the selected Python environment. The updater now uses a checked-in standard-library-only regression runner and that runner changes working directory to the extracted source root before loading tests, preventing repository-relative checks from accidentally reading an existing checkout. Firmware runtime source differs from Build 189 only in `FW_BUILD`.

## External-audit finding closure

| Audit item | Final status |
|---|---|
| A-01 TEXT out-of-bounds minimum length | **Closed**; guard requires global header plus first marker byte; regression coverage present |
| A-02 buzzer qualification status | **Closed**; low-trigger HIGH-idle behavior physically verified, module remains cool at idle and functions correctly |
| A-03 stale Wiki troubleshooting / RC wording | **Closed**; current operational guidance aligned with 0.5.2 stable state |
| A-04 full clean PlatformIO build | **Not reproduced in packaging environment**; PlatformIO/Arduino CLI unavailable, so no fresh four-environment compile is claimed |
| A-05 software clock after ~49.7 days | **Closed**; 64-bit `esp_timer_get_time()` monotonic epoch plus regression test |
| A-06 updater stale-object risk | **Closed** for selected environment; update path invalidates `.pio/build/$PIO_ENV` and runs tests before sync |
| A-07 explicit accelerometer disable | **Closed** with `IDOTMATRIX_ACCEL_DRIVER_NONE` |
| A-08 buzzer regression reproducibility | **Closed/improved** with checked-in host-testable idle-polarity policy and physical qualification |
| A-09 LIS3DH dependency range | **Closed**; exact `1.3.0` pin |
| A-10 non-English source comments | **Closed** |

## Hardware evidence carried into final

ESP32-C3 qualification includes:

- passive low-level-trigger buzzer on GPIO3 at 3.3 V, correct sound and no appreciable idle heating;
- DS3231 retained time, BLE writeback and hot recovery;
- persisted Carousel boot priority over RTC Clock;
- no-Carousel/no-RTC screen-off fallback;
- Clock style/24-hour/date/color persistence;
- Alarm and Program/Schedule operation after app disconnect and board reset;
- Countdown completion buzzer and BLE connection beep;
- ICM-20689 automatic orientation on the tested shared-I2C configuration;
- post-hardening TEXT, Alarm, Carousel and Clock smoke tests.

## Final package policy

- current documentation identifies `0.5.2 / Build 190`;
- development and release-candidate identifiers remain only in explicitly historical material;
- intermediate 0.5.2 RC release-note/audit files are removed from the public package and consolidated into the final release notes/audit plus `HISTORY.md`;
- all documented board, matrix and peripheral images are retained;
- local hardware overrides, generated firmware, build output and Python caches are excluded.

## Toolchain limitation

The packaging environment does not provide PlatformIO, Arduino CLI, cppcheck or clang-tidy. The final audit therefore does not claim a fresh four-target toolchain build or dedicated C++ static-analyzer pass from this environment. This limitation does not alter the recorded hardware qualification and repository regression results, but should remain explicit.

## Verification results

Final-tree and extracted-archive checks:

- repository regression suite: **29/29 PASS**;
- `update_idotmatrix_emulator.sh`: `bash -n` **PASS**;
- dependency-free `tests/run_tests.py` gate: **29/29 PASS** under `python3 -S`, without site-packages or pytest;
- regression runner working directory: extracted source root before module loading: **PASS**;
- source/Wiki relative Markdown links/images: **54 checked, 0 broken**;
- GitHub Wiki page links: **81 checked, 0 broken**;
- current non-historical documentation contains no 0.5.2 development/release-candidate identifiers: **PASS**;
- intermediate 0.5.2 RC release-note/audit files absent from final package: **PASS**;
- source hardware/demo assets: **9 files**, preserved byte-for-byte from Build 189;
- Wiki hardware/demo images: **9 files**, preserved byte-for-byte from Build 189;
- archive hygiene before test execution: no `.pio`, `.pytest_cache`, `__pycache__`, `.pyc`, `.bin`, `.elf`, `.map`, `.hex` or local `src/IDotMatrixUserConfig.h`: **PASS**;
- B188 -> B189 runtime source diff: only `FW_RELEASE` and `FW_BUILD`: **PASS**.

The extracted archive tests were executed with Python bytecode/cache generation disabled so package-hygiene checks remain distinguishable from files created by the test runner itself.

## Final assessment

All code, documentation and packaging findings from the external pre-release audit that require repository changes are closed in the final 0.5.2 source. The only audit item not independently reproduced in the packaging environment is the clean four-environment PlatformIO compile, because the required toolchain is unavailable there. No claim of that compile is made.

Given the recorded hardware qualification, post-hardening smoke tests, dependency-free regression suite, exact B189 -> B190 runtime diff and package/documentation checks above, Build 190 is the stable 0.5.2 release source state.
