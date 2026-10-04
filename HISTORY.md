## 0.6.0-dev.3 / Build 201

One-item Carousel GIF look-ahead prefetch on top of the hardware-qualified B200 persistent compressed-source cache.

- Enables prefetch only on the three Waveshare profiles; other targets retain B200 behavior.
- Prefetches only the immediate next Carousel slot when it is a cache-eligible GIF.
- Starts after a 250 ms grace period and copies at most 4 KiB per loop iteration, after active GIF/TEXT rendering.
- Computes CRC32 during the incremental copy and transfers buffer ownership to the B200 cache only after exact size/CRC validation.
- Preserves the 1 MiB cache budget, 512 KiB entry cap, 12-entry LRU policy, 2 MiB transient staging cap and 4 MiB PSRAM reserve.
- Cancels partial work on Carousel stop/advance or target metadata change; normal B200 cold playback remains the fallback.
- Adds `[GIFPREFETCH]`, `gif.prefetch.*` memory snapshots and `gif.prefetch.total_us` telemetry.

Status: **development build; host regression/compile-stub qualified, requires Waveshare PlatformIO compile and physical prefetch-transition qualification.**

## 0.6.0-dev.3 / Build 200

Persistent compressed-GIF source cache build on top of the hardware-qualified B199 transient staging path.

- Adds a Waveshare-only 1 MiB persistent PSRAM source cache with 512 KiB per-entry cap and at most 12 entries.
- Keys entries by path + byte size + CRC32 and uses LRU eviction.
- Protects the active decoder entry from eviction until `AnimatedGIF::close()` completes.
- Adopts the B199 staged allocation directly into the cache with no second allocation/copy.
- Warm Carousel/Preset hits bypass repeated LittleFS CRC rereads and serve the decoder directly from PSRAM.
- Invalidates cached sources on live/event/Carousel/Preset file replacement or clear.
- Retains B199 transient staging, 2 MiB stage cap, 4 MiB PSRAM reserve and transparent LittleFS fallback.
- Adds `[GIFCACHE]` and `gif.cache.*` diagnostics. No prefetch is introduced yet.

Status: **hardware-qualified.** Cold insert, warm hit, Preset/Carousel invalidation, cache re-population, 12-entry pressure and real LRU eviction/reuse were verified on the Waveshare target.

## 0.6.0-dev.3 / Build 199

Waveshare transient whole-GIF PSRAM staging build on top of the hardware-qualified B198 BLE/filesystem hardening baseline.

- Adds best-effort staging of one complete compressed GIF source in PSRAM on all three Waveshare profiles.
- Uses a 2 MiB per-source cap and preserves a 4 MiB PSRAM reserve; staging also requires a sufficiently large contiguous PSRAM block.
- Keeps LittleFS authoritative and falls back transparently to the B198 file-backed AnimatedGIF callback path when staging is disabled or any guard/allocation/copy step fails.
- Keeps one decoder/source path: the existing AnimatedGIF open/read/seek/close callbacks now serve either the staged PSRAM source or LittleFS.
- Frees the staged buffer only after `AnimatedGIF::close()`/decoder teardown. No persistent cache or Carousel prefetch is introduced yet.
- Adds `[GIFSTAGE]`, `gif.stage.*` memory snapshots and `gif.stage.copy_us` latency telemetry. First-frame latency now includes the staging copy while decoder-open latency remains separately measured.
- Retains B198 deferred Preset/Carousel filesystem publication and all qualified OTA/scaling behavior.

Status: **hardware-qualified for Carousel and live GIF PSRAM staging/release with repeated soak and no observed crash; deliberate >2 MiB fallback was not physically exercised in the captured trace.**

## 0.6.0-dev.3 / Build 198

Compile-only corrective build for the B197 deferred Preset/Carousel filesystem hardening.

- Preserves the B197 runtime design: final Preset/Carousel publication remains deferred from `nimble_host` to `loopTask`, with the final ACK sent only after commit.
- Fixes the PlatformIO/Arduino `.ino` auto-prototype failure where `queueDeferredAssetCommit(DeferredAssetCommitKind, ...)` was prototyped before `DeferredAssetCommitKind` was declared.
- Changes only that helper boundary to accept a validated `uint8_t` and cast to the enum inside the implementation.
- Adds a host regression guard specifically for Arduino-safe helper signatures using custom enum state.
- No protocol, storage, allocation, OTA, scaling, BLE library or memory-optimization behavior changes are introduced.

Status: **hardware-qualified.** The real Waveshare build/upload succeeded; repeated Preset TEXT/font commits and Carousel commits completed without the B196 `nimble_host` stack-canary reboot, and playback continued after commit.

## 0.6.0-dev.3 / Build 197

Targeted Waveshare BLE/filesystem hardening after a physical B196 Preset TEXT/font upload triggered `Stack canary watchpoint triggered (nimble_host)`.

- Defers final Preset and Carousel staged-file flush/close/commit/cleanup from the FA02 NimBLE callback to the Arduino loop task.
- Sends the final bulk completion ACK only after the deferred filesystem publication has completed and protocol state has been reset.
- Preserves an already-complete pending commit across BLE disconnect.
- Retains B196 memory/latency telemetry and all B194/B195 OTA/scaling behavior.
- Does not yet introduce GIF PSRAM staging/cache/prefetch.

Status: **development build; requires physical reproduction test of the B196 Preset crash and repeated Carousel/Preset uploads.**

## 0.6.0-dev.3 / Build 196

- Measurement-only memory/latency baseline on the physically qualified B195 Waveshare runtime.
- Enables structured internal-DRAM, DMA-capable heap and PSRAM snapshots only for the three Waveshare profiles.
- Instruments boot, logical-buffer allocation, storage, HUB75, BLE, TEXT, RAW/Graffiti, GIF, Carousel, Schedule PNG and OTA without changing their allocation/storage policies.
- Adds tagged microsecond latency records for TEXT render, GIF open/first frame, Carousel start, Schedule PNG decode and OTA upload.
- Adds a dependency-free serial-log summarizer and regression guards for telemetry scope/format.
- Does not introduce PSRAM placement, GIF staging/cache or Carousel prefetch.

Status: **physical 64x64 trace captured; memory/GIF baseline recorded and Preset upload exposed the `nimble_host` stack failure addressed by B197.**

## 0.6.0-dev.3 / Build 195

- Scaling-qualification build on the physically exercised Build 194 Waveshare baseline.
- Adds dedicated Waveshare logical 16x16 and 32x32 PlatformIO environments while retaining the existing logical 64x64 environment and one physical 64x64 HUB75 panel.
- Keeps the existing nearest-neighbour logical-to-physical scaler unchanged; expected replication is 4x4 for 16x16, 2x2 for 32x32 and 1:1 for 64x64.
- Adds deterministic visual scaling qualification PNGs and host regression coverage for all three profile configurations.
- Extends startup diagnostics with explicit screen type and updates the repository helper to recognize every `waveshare_s3_rgbmatrix_*` environment as Waveshare.
- Inherits the B194 OTA implementation unchanged; physical testing confirmed successful upload, recovery to the previous firmware after an interrupted upload, and preserved persistent memory/state.
- No PSRAM/media optimization or deferred on-board peripheral support is introduced.

Status: **hardware qualified for Waveshare logical 16x16, 32x32 and 64x64 scaling; superseded by B196 measurement work.**

## 0.6.0-dev.3 / Build 194

- OTA usability hardening on top of the Build 193 Waveshare baseline.
- Adds the core ESP32 `DNSServer` while OTA maintenance mode is active; normal runtime still keeps Wi-Fi off.
- Wildcard DNS points captive-connectivity requests to the SoftAP.
- Redirects common Android, Apple and Windows captive probe paths plus unknown HTTP paths to the OTA upload page.
- Keeps `/health` and direct `http://192.168.4.1/` access available.
- Keeps the existing dual-slot `Update` upload transaction and interrupted-upload handling unchanged.
- Adds host regression guards and compile stubs for captive DNS/redirect behavior.
- No scaling, GIF/PSRAM, storage or peripheral changes are included.

Status: **hardware-qualified for OTA upload, interrupted-upload recovery and persistent-state preservation; superseded by later development builds.**

## 0.6.0-dev.3 / Build 193

- Supersedes Build 192 after the Waveshare PlatformIO compile gate failed with `BLEDevice.h: No such file or directory`.
- Keeps the Build 192 WLED-qualified Tasmota Arduino 3.3.8 / ESP-IDF 5.5.4 platform and 32 MB partition geometry unchanged.
- Adds pinned `h2zero/NimBLE-Arduino @ 2.5.1` only to the Waveshare environment, matching the BLE library family already used by the qualified iDotMatrix WLED Usermod.
- Adds `IDOTMATRIX_USE_NIMBLE=1` for the Waveshare target and conditional NimBLE 2.x server, characteristic callback, GATT and advertising code.
- Preserves the existing legacy Arduino BLE backend for MatrixPortal, ESP32-C3 and classic ESP32 profiles.
- Keeps the 517-byte local MTU and the existing FA/AE services, characteristics, manufacturer data and delayed advertising-restart behavior.
- Adds regression guards for the pinned NimBLE dependency/backend selection.
- No GIF staging/cache, SD, PCF85063, QMI8658, SHTC3 or audio features are introduced.

Status: **development build; requires PlatformIO compilation and physical Waveshare qualification.**

## 0.6.0-dev.2 / Build 192

- Supersedes Build 191 after its first Waveshare hardware gate produced a blank panel and repeated native-USB reconnects/reset-loop symptoms.
- Waveshare target now uses the same Tasmota Arduino 3.3.8 / ESP-IDF 5.5.4 platform baseline as the already-qualified WLED Waveshare environment.
- Restores the WLED-qualified 32 MB partition geometry: 3 MiB `ota_0`, 3 MiB `ota_1`, filesystem at `0x610000`, and 64 KiB coredump at the end of flash.
- Removes the explicit `flash_mode=opi` override; the `esp32s3camlcd` board keeps OPI boot through `memory_type=opi_opi` with DOUT flash access.
- Adds serial checkpoints around HUB75 initialization for physical bring-up diagnostics.
- No GIF staging/cache, SD, PCF85063, QMI8658, SHTC3 or audio features are introduced.

## 0.6.0-dev.1 / Build 191

First development build of the 0.6.0 line, based on the stable 0.5.2 / Build 190 runtime.

- Adds the standalone `waveshare_s3_rgbmatrix_64x64` target for Waveshare ESP32-S3 RGB Matrix / ESP32-S3-N32R16.
- Adds the official/WLED-qualified Waveshare HUB75 GPIO mapping.
- Configures 32 MB flash, 16 MB octal PSRAM and a dedicated two-slot OTA partition table.
- Adds a physically triggered OTA maintenance mode using BOOT/GPIO0 and a local HTTP firmware-upload page.
- Keeps Wi-Fi disabled during normal operation until OTA maintenance is explicitly triggered.
- Adds startup diagnostics for board identity, flash, PSRAM and OTA state.
- Deliberately leaves PCF85063, QMI8658, SHTC3, MicroSD and audio support disabled for this first standalone Waveshare bring-up.
- Adds regression coverage for the Waveshare target, pinout, OTA update path and 32 MB partition layout.

Status: **development build; requires PlatformIO compilation and physical Waveshare qualification.**

## 0.5.2 / Build 190

Stable packaging/tooling correction that supersedes Build 189.

- Keeps the qualified 0.5.2 firmware runtime unchanged apart from the build identifier.
- Replaces the updater's implicit `pytest` dependency with the checked-in standard-library-only `tests/run_tests.py` regression runner.
- Forces that runner to execute from the extracted archive root, so tests using repository-relative paths cannot accidentally inspect the pre-existing checkout.
- Allows the update workflow to run with the Python interpreter already available in the PlatformIO environment, without installing pytest into that environment.
- Adds regression coverage that prevents the updater from reintroducing a mandatory pytest dependency.
- Preserves all controller-board, LED-matrix and peripheral-module documentation assets.

Status: **stable public release; supersedes Build 189.**

## 0.5.2 / Build 189

Stable release promoted from the hardware-smoke-tested 0.5.2-rc.5 / Build 188 baseline.

- Promotes the completed 0.5.2 hardware and audit hardening work to stable release status.
- Keeps the Build 188 runtime behavior unchanged apart from release/build identity.
- Consolidates current documentation around the stable 0.5.2 release.
- Removes intermediate 0.5.2 release-candidate audit/note files from the public package while retaining their chronology below.
- Preserves the complete controller-board, LED-matrix and peripheral-module hardware image catalog.
- Records the completed ESP32-C3 smoke tests for TEXT, Alarm, Carousel and Clock after the final hardening changes.

Status: **stable public release.**

## 0.5.2-rc.5 / Build 188

Hardening candidate based on the RC4/B187 runtime.

- Fixed the TEXT parser minimum-length off-by-one so a 14-byte malformed payload cannot read `data[14]`.
- Replaced the software-clock `millis()` epoch with the 64-bit ESP timer monotonic timebase, removing the ~49.7-day fallback-clock wrap limitation.
- Added `IDOTMATRIX_ACCEL_DRIVER_NONE` so local configuration can explicitly suppress a profile-provided accelerometer backend.
- Pinned Adafruit LIS3DH to exact version `1.3.0`.
- Hardened the update helper: extracted-source tests run before synchronization, dirty Git state requires explicit confirmation, and the selected PlatformIO environment build directory is invalidated before build/upload.
- Added host-regression guards for TEXT minimum length and passive-buzzer idle polarity.
- Recorded hardware qualification of the RC3 low-trigger HIGH-idle buzzer correction: the 3.3 V GPIO3 module remains cool at idle and sounds correctly.
- Recorded successful on-device Alarm and Program/Schedule operation after app disconnect and board reset, plus Carousel-first boot, Countdown buzzer, BLE connection beep, RTC hot recovery and the no-Carousel/no-RTC screen-off fallback.
- Refreshed current Wiki troubleshooting and preserved the complete controller, matrix and peripheral image catalog.

## 0.5.2-rc.4 / Build 187

- Packaging/documentation correction of RC4/B186; firmware runtime behavior is unchanged apart from the build signature.
- Restores the controller-board and LED-matrix reference photographs that were accidentally omitted during earlier Wiki cleanup.
- Restores the original image placements for MatrixPortal S3, ESP32-C3 SuperMini, 64x64 HUB75 and 16x16 WS2812 hardware.
- Keeps the newer buzzer, DS3231/AT24C32 and GY-521 module photographs added in RC4/B186.
- Adds the controller/matrix photographs to `docs/HARDWARE-SUPPORT.md` as well as the Wiki.

Status: **RC4 packaging correction; supersedes Build 186.**

## 0.5.2-rc.4 / Build 186

- Documentation-only release-candidate refresh from RC3/B185; runtime behavior is intentionally unchanged apart from the release/build signature.
- Added an exact hardware-module catalog with supplied reference photos.
- Documented the qualified three-wire S9012 passive low-level-trigger buzzer module.
- Documented the qualified DS3231 + AT24C32 combination board, including the fact that the firmware currently uses only the DS3231 RTC portion.
- Added the vendor-described GY-521 / MPU-6050 board as an implemented but not yet hardware-qualified orientation candidate.
- Clarified that `WHO_AM_I`, not board silkscreen or seller title, is authoritative for distinguishing MPU-6050 from ICM-20689.
- Updated README, hardware support, orientation documentation and Wiki hardware pages accordingly.

## 0.5.2-rc.3 / Build 185

Passive buzzer electrical-idle correction release candidate.

- adds `IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW` for passive buzzer modules whose transistor input is active-low;
- configures the qualified ESP32-C3 reference module on GPIO3 as low-level-trigger, preserving the existing 2000 Hz LEDC tone;
- drives the passive output to its inactive logic level while silent instead of relying on a zero-frequency LEDC tone;
- for the qualified three-wire module, idle is HIGH so the onboard PNP transistor and buzzer are not continuously DC-biased;
- establishes the safe idle GPIO level before LEDC attachment to minimize startup activation;
- keeps direct active-high passive buzzers and self-oscillating active buzzer modules supported through configuration;
- leaves RTC, Carousel boot priority, Clock persistence, BLE protocol, Graffiti, media, Audio/Rhythm and orientation behavior unchanged.

Status: **release candidate; low-level-trigger passive buzzer behavior requires the final on-device RC3 smoke test.**

## 0.5.2-rc.2 / Build 184

Second release candidate for the 0.5.2 line.

- corrects boot-display priority so a valid persisted Device Assets/Carousel resumes before the optional RTC-backed Clock;
- keeps the fallback order `stored Carousel -> valid RTC Clock -> screen off`;
- adds a static regression test that locks this priority order;
- updates current release documentation and validation material to RC2;
- makes no other intended runtime change from the hardware-tested RC1/B183 baseline.

Status: **release candidate; requires the normal on-device RC2 smoke test before final 0.5.2 publication.**

## 0.5.2-rc.1 / Build 183

First release candidate for the 0.5.2 line.

- promotes the hardware-tested Build 182 baseline to RC1 without new user-facing features;
- keeps DS3231 RTC recovery, battery-backed boot time and Clock presentation persistence;
- keeps the hardware-qualified passive buzzer and ICM-20689 ESP32-C3 paths;
- removes release-facing development chronology from operational documentation and consolidates it here;
- replaces the obsolete 0.5.0 release-validation page with the 0.5.2 RC validation scope;
- removes unused RTC configuration bookkeeping and retains all protocol/debug switches disabled by default;
- removes internal Build 173-182 note/audit files from the public RC package after consolidating relevant information into HISTORY, release notes and the RC audit.

Status: **release candidate; final publication still requires the Build 183 on-device smoke test.**

# Release History

## 0.5.2-dev / Build 182

Pre-RC Clock-persistence regression fix.

- persists Clock style, 12/24-hour mode, date visibility and RGB text colour in NVS;
- restores those values before the RTC boot policy renders the first Clock frame;
- coalesces repeated app Clock commands with a one-second deferred save to limit flash wear;
- clears the persisted Clock presentation as part of the existing device reset;
- adds a dedicated static regression test for Clock persistence and boot ordering;
- leaves RTC timekeeping, BLE protocol framing, renderers, media, Audio/Rhythm, orientation and buzzer behavior otherwise unchanged.

Status: **replacement pre-RC baseline for the final `0.5.2-rc.1` physical regression.**

## 0.5.2-dev / Build 181

Pre-RC consolidation, RTC recovery and documentation-audit build.

- records successful DS3231 hardware qualification on the ESP32-C3 shared-I2C reference hardware, including battery-backed retention, BLE time writeback and cold boot directly into Clock;
- retries a configured but unavailable RTC every 60 seconds by default (`IDOTMATRIX_RTC_RETRY_INTERVAL_MS`), with an immediate loop-side reprobe requested after a valid BLE time sync;
- if a recovered RTC is invalid while the software clock is already synchronized, writes the current software time to the RTC automatically;
- seeds the software clock from a valid RTC at boot/recovery, preserving a time fallback across temporary later I2C failures;
- routes runtime RTC read failures back into the same recovery path;
- reduces Build 180 startup diagnostics to one final hardware summary now that C3 native USB CDC routing is qualified;
- removes deprecated manual `BLE2902` descriptors under the pinned Arduino-ESP32 3.3.11/NimBLE stack, relying on the framework-generated CCCD for notify characteristics;
- keeps DS3231 direct-I2C access, ICM-20689, passive buzzer, Graffiti, media, audio and renderer behavior otherwise unchanged;
- audits README, hardware configuration, PlatformIO guidance, Wiki qualification tables and historical release-validation wording.

Status: **consolidated 0.5.2-dev candidate baseline; intended to advance to `0.5.2-rc.1` after final physical regression.**

## 0.5.2-dev / Build 180

ESP32-C3 native-USB serial routing and RTC boot-diagnostics build.

- enables `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1` in the `esp32c3_ws2812_16` PlatformIO profile so firmware `Serial` output is routed to the native USB CDC/JTAG port;
- adds a late startup summary reporting user-config presence, shared-I2C pins, RTC backend/address/readiness/validity/status/current time, software time-sync state, selected boot display mode and screen power state;
- repeats the summary twice after setup to survive late ESP32-C3 USB monitor attachment;
- keeps the Build 179 DS3231 driver and boot policy unchanged; the driver already clears DS3231 OSF after a successful BLE time synchronization;
- documents that Arduino IDE builds do not inherit PlatformIO hardware defaults and therefore require explicit local hardware configuration when those defaults are needed.

Status: **diagnostic build for closing DS3231 boot qualification; no intended RTC runtime-policy change.**

## 0.5.2-dev / Build 179

RTC compile-fix build.

- closes the `IDOTMATRIX_RTC_AVAILABLE` preprocessor block in `setup()` correctly;
- fixes the `unterminated #if` compilation failure introduced by Build 178;
- adds a static preprocessor-balance regression check;
- makes no intended runtime change to the DS3231, I2C, ICM-20689, buzzer, BLE, Graffiti or display behavior.

Status at Build 179: **compile-fix successor to Build 178; RTC hardware qualification still pending at that time.** Qualification was completed on ESP32-C3 and recorded in Build 181.

## 0.5.2-dev / Build 178

DS3231 RTC/shared-I2C integration build.

- promotes the dormant DS3231 concept into the common hardware-configuration layer;
- enables the DS3231 by default on the `esp32c3_ws2812_16` reference profile;
- configures that C3 profile for the shared I2C bus on SDA GPIO1 / SCL GPIO2;
- replaces the former RTClib-dependent path with a direct DS3231 register driver that never calls `Wire.begin()` internally;
- initially gave an already valid RTC boot priority over stored Carousel content; this ordering was corrected in `0.5.2-rc.2 / Build 184` so persistent Carousel resumes first;
- rejects an oscillator-stop/invalid RTC until a valid BLE time synchronization arrives;
- updates the RTC from the official-app time-sync command and clears the oscillator-stop flag;
- lets Clock, Alarm, Program/Schedule and ECO timing use persistent RTC time after MCU reboot;
- protects DS3231 address `0x68` from an explicitly configured MPU-family accelerometer collision and makes auto-probe prefer `0x69` when the RTC is enabled;
- keeps the Build 177 passive-buzzer and qualified ICM-20689 paths unchanged.

Status at Build 178: **implemented and statically validated; DS3231 emulator hardware qualification pending at that time.** Qualification was completed on ESP32-C3 and recorded in Build 181.

## 0.5.2-dev / Build 177

Passive-buzzer hardware support build.

- adds a generic buzzer backend selection: `NONE`, `ACTIVE` or `PASSIVE`;
- implements passive buzzer output through the ESP32 LEDC peripheral, keeping the existing non-blocking trill state machine;
- keeps active self-oscillating buzzer support available for other hardware;
- configures the qualified `esp32c3_ws2812_16` profile for a passive buzzer on GPIO3 at 2000 Hz;
- enables Alarm, Countdown, Program/Schedule and connection notification buzzer policies on that C3 reference profile;
- exposes buzzer type, pin, frequency, active polarity and per-event policy through `IDotMatrixUserConfig.h`;
- permits an explicit local `IDOTMATRIX_BUZZER_NONE` selection to disable profile buzzer defaults cleanly;
- leaves BLE protocol, media handling, display rendering and the qualified orientation paths unchanged.

Status: **hardware-qualified on the ESP32-C3 reference hardware with the passive buzzer on GPIO3.**

## 0.5.2-dev / Build 176

Post-qualification cleanup and documentation-alignment build.

- records successful ICM-20689 hardware qualification on ESP32-C3 with the I2C bus shared with the gesture sensor;
- keeps the qualified ICM-20689 register path, auto-probe logic and common orientation engine unchanged;
- returns verbose orientation probe/configuration and continuous XYZ diagnostics to opt-in operation;
- removes the unused sample-diagnostic timestamp from builds where continuous XYZ logging is disabled;
- retains one concise serial error if an enabled orientation sensor fails to initialize;
- keeps MPU-6050 support implemented but explicitly unqualified;
- aligns README, hardware support, PlatformIO guidance and Wiki status with the physical B175 test;
- leaves BLE, Graffiti, media, audio, display ownership and renderer behavior unchanged.

Status: **ICM-20689 hardware-qualified on ESP32-C3 shared-I2C hardware; MPU-6050 remains unqualified.**

## 0.5.2-dev / Build 175

Qualification-diagnostics build for ESP32-C3 + external ICM-20689 on a shared I2C bus.

- keeps the Build 174 sensor driver, I2C initialization and orientation behavior unchanged;
- enables one-shot orientation diagnostics by default whenever an accelerometer backend is selected;
- records the `0x68` and `0x69` probe attempts separately, including ACK, `WHO_AM_I`, backend match and final configuration state;
- prints the configured I2C pins (or reports board-default pins);
- repeats the complete orientation diagnostic summary near the end of `setup()` so ESP32-C3 USB/CDC monitors that attach late do not miss the early probe;
- exposes the diagnostic switches in `IDotMatrixUserConfig.example.h`; continuous XYZ sample logging remains opt-in;
- no intended changes to BLE, Graffiti, display rendering, audio, media, orientation classification or sensor register programming.

Status: **diagnostic build for identifying the ESP32-C3 shared-bus qualification failure without changing the working WLED-derived ICM-20689 logic.**

## 0.5.2-dev / Build 174

Configuration/persistence build for external orientation-sensor qualification.

- added optional `src/IDotMatrixUserConfig.h`, created from the tracked `.example.h` template;
- local configuration can select the accelerometer backend and define SDA/SCL, sensor address and mount rotation;
- changed PlatformIO sensor flags to `IDOTMATRIX_DEFAULT_*` fallbacks so explicit local values take precedence without duplicate backend definitions;
- added compile-time validation and regression coverage for local-config precedence;
- `update_idotmatrix_emulator.sh` now preserves the local hardware header across destructive source synchronization;
- no intended runtime changes to the Build 173 ICM-20689/MPU-family driver or to qualified BLE/display paths.

Status: **ready for ICM-20689 hardware qualification with board-specific wiring kept outside tracked source files.**

## 0.5.2-dev / Build 173

First hardware-compatibility development build after the 0.5.1 release.

- added a dedicated `IDOTMATRIX_ACCEL_DRIVER_ICM20689` backend based on the already hardware-tested WLED implementation supplied for the external module;
- identifies ICM-20689 with `WHO_AM_I=0x98`;
- added the shared MPU-family register path for MPU-6050 (`WHO_AM_I=0x68/0x69`) without claiming hardware qualification;
- configures 50 Hz sampling, +/-2 g acceleration and the ICM-20689-specific accelerometer DLPF register at `0x1D`;
- verifies critical configuration registers after initialization;
- supports automatic probing of I2C addresses `0x68` and `0x69` when `IDOTMATRIX_ACCEL_I2C_ADDRESS=0`;
- added optional compile-time `IDOTMATRIX_I2C_SDA_PIN` / `IDOTMATRIX_I2C_SCL_PIN` overrides for external sensor wiring;
- added `matrixportal_s3_hub75_64_icm20689` as a development/qualification PlatformIO profile with continuous sample diagnostics enabled;
- left the qualified LIS3DH backend, common orientation engine, BLE protocol, Graffiti, media, audio and renderer paths unchanged.

Status at Build 173: **ICM-20689 implementation complete, hardware qualification pending.** Qualification was later completed on ESP32-C3 and is recorded in Build 176.

## 0.5.1 / Build 172

Final 0.5.1 release of the standalone iDotMatrix ESP32 Emulator.

Highlights:

- added the hardware-qualified MatrixPortal S3 LIS3DH automatic-orientation subsystem;
- kept rotation in the final logical-to-physical output mapping so existing renderers remain unchanged;
- added generic `IDOTMATRIX_ACCEL_MOUNT_ROTATION=0|90|180|270` compensation for custom sensor mounting;
- reconstructed the official 64x64 Graffiti full-raster protocol from an Android Bluetooth HCI capture against original iDotMatrix hardware;
- implemented the dedicated 9-byte Graffiti `type=0x00` state machine with 4096-byte RGB chunks and original-device ACK semantics (`0x02` while incomplete, `0x01` on completion);
- isolated Graffiti `type=0x00` from normal 16-byte GIF/RAW/TEXT Bulk (`0x01..0x03`);
- disabled Graffiti protocol tracing in the final source;
- changed the default PlatformIO target to the primary MatrixPortal S3 / 64x64 HUB75 environment;
- aligned README, protocol notes, release validation and Wiki material with the final release state.

The qualified 0.5.0 / Build 162 behavior remains the regression baseline for existing features.

## 0.5.0 / Build 162

Final 0.5.0 release of the standalone iDotMatrix ESP32 Emulator.

Highlights:

- hardware-qualified Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75 target;
- hardware-qualified ESP32-C3 + 16x16 WS2812 target;
- logical 16x16, 32x32 and 64x64 display profiles with resolution-independent output scaling;
- Clock, Countdown, Stopwatch and Scoreboard rendering aligned with original-device references;
- TEXT support for 16/32/64-pixel glyph families, scrolling, paging and multiline static layouts;
- image/GIF playback, Graffiti/DIY, persistent Device Assets Carousel and volatile Preset/Default playback;
- Alarm and Program/Schedule multipart media handling with CRC32 validation;
- five LEVEL and five FFT Audio/Rhythm effects with dedicated audio stream framing;
- verified TEXT/Preset/Carousel display ownership isolation;
- LittleFS media storage and transactional replacement paths;
- cleaned public documentation and release metadata.

The 16x16 Stopwatch animation was revalidated frame-by-frame against the supplied original-device reference video before this release. No additional visual change was required.

## 0.4.0 / Build 118

Previous stable standalone emulator release. It established the original WS2812-focused baseline before the larger-display, multipart-media, Preset and MatrixPortal/HUB75 work included in 0.5.0.

# Internal development history leading to 0.5.1

## 0.6.0-dev / Build 171

- Reconstructed the 64x64 Graffiti full-raster protocol from a Bluetooth HCI capture against original iDotMatrix hardware.
- Confirmed a dedicated 9-byte Graffiti header: packet length at bytes 0..1, type `0x00`, marker `0x00` on the first packet / `0x02` on continuation packets, total raster size at bytes 5..8, then RGB payload at byte 9.
- Confirmed 4096-byte raster chunks for the captured 64x64 transfer (`12288 = 64 * 64 * 3`, three chunks).
- Confirmed original-hardware acknowledgement semantics: `05 00 00 00 02` while incomplete and `05 00 00 00 01` on completion.
- Added a dedicated Graffiti raster state machine, separate from the normal 16-byte GIF/RAW/TEXT Bulk parser; no CRC field is assumed for this transport.
- Publishes the completed RGB raster to `DISPLAY_GRAFFITI` only after all bytes have been received.
- Added timeout/disconnect/reset cleanup for partial Graffiti raster transfers.

## 0.6.0-dev / Build 170

- Kept the observed 4105-byte Graffiti type-`0x00` packet outside the generic Bulk transaction state, preserving the Build 169 routing fix.
- Added the intermediate type-0 transfer acknowledgement `05 00 00 00 01` required by the official app to continue after the auxiliary packet.
- The acknowledgement is stateless: the packet does not allocate RAW storage, does not set `bulk.active`, and cannot intercept later type-`0x02` raster image packets or normal control commands.
- Added explicit `GRAFFITI TYPE0 ACK TX` diagnostics for hardware validation.
- No changes to the qualified type-`0x02` RAW RGB, GIF, TEXT, Carousel, Preset, Alarm, Schedule, audio or orientation paths.

## 0.6.0-dev / Build 169

- Cross-checked the Graffiti/image paths against independent iDotMatrix reverse-engineering repositories. Hardware-validated community implementations consistently document live DIY/Graffiti as command `0x05/0x01` and full raster image upload as normal Bulk type `0x02` with 4096-byte chunks, CRC32 and intermediate/final ACKs.
- Reclassified the observed 4105-byte type-`0x00` / 12288-byte packet as an isolated app-specific auxiliary Graffiti packet rather than a generic Bulk transfer.
- The type-0 auxiliary packet is now consumed without opening `bulk.active` and without emitting a generic transfer ACK, so it can no longer hijack subsequent canonical type-2 image packets or unrelated control commands.
- Restored the generic RAW RGB path to type `0x02` only.
- Retained narrow diagnostics for the type-0 auxiliary packet while leaving all validated GIF/TEXT/Carousel/Preset/Alarm/Schedule, audio and orientation paths unchanged.

## 0.6.0-dev / Build 168

- Reworked the Graffiti type `0x00` diagnostic path so every logical packet received after a Graffiti transfer starts is captured before normal Bulk-header validation.
- Continuation packets are dumped regardless of their apparent `type`/header fields, preventing the generic Bulk parser from hiding the alternate Graffiti continuation envelope.
- Logs packet length, declared logical length, bytes 2/3 and the first 96 raw bytes of each continuation packet.
- Sends an intermediate type-0 transfer ACK after each captured continuation packet so the app can continue the diagnostic transfer.
- Leaves the validated type-2 RAW RGB path and all non-Graffiti subsystems unchanged.
- Diagnostic build only: it intentionally does not publish the Graffiti framebuffer yet.

## 0.6.0-dev / Build 167

- Added a diagnostic-only trace for Graffiti bulk type `0x00` continuation packets.
- Dumps the parsed continuation header plus the first 64 bytes of each logical type-0 packet.
- On a type-0 continuation header mismatch, keeps the transaction open and sends an intermediate ACK so the mobile app can continue transmitting enough packets to reveal the real multipart framing.
- Leaves the validated type-2 RAW RGB transfer path unchanged.
- This build is intended for protocol capture; it does not claim a final Graffiti multipart fix.

## 0.6.0-dev / Build 166

- Added a dedicated Graffiti full-frame RAW RGB path for multipart Bulk type `0x00`.
- Type `0x00` is classified as Graffiti RAW only when `total == MATRIX_WIDTH * MATRIX_HEIGHT * 3`; other type-0 traffic is not reclassified.
- Graffiti RAW chunks now use the existing transaction CRC, intermediate/final ACK and framebuffer publication path already used by confirmed RAW RGB transfers.
- Added completion diagnostics for the new Graffiti RAW path.
- No LIS3DH/orientation, audio, GIF, TEXT, Carousel, Preset, Alarm or Schedule behavior changed.

## 0.6.0-dev / Build 165

- Consolidated the LIS3DH backend and common orientation support layer.
- Added `IDOTMATRIX_ACCEL_MOUNT_ROTATION` with compile-time validation for `0`, `90`, `180` and `270` degrees clockwise.
- Mount compensation is sensor/board agnostic, allowing the same accelerometer backend to be reused on custom boards or as an external module without changing the orientation engine.
- MatrixPortal S3 explicitly uses a `0` degree mount offset, preserving the hardware-qualified Build 164 behavior.
- No renderer, BLE protocol, media, audio, automation or display-mode behavior changed.

## 0.6.0-dev / Build 164

First automatic-orientation build.

- qualified the MatrixPortal S3 physical axis map from Build 163 measurements: `+Y`=0 deg, `+X`=90 deg clockwise, `-Y`=180 deg and `-X`=270 deg clockwise;
- enabled automatic display rotation through the common orientation engine;
- applied rotation only at the final logical-to-physical output stage so all existing renderers remain unchanged;
- static framebuffers are refreshed immediately when a stable orientation changes;
- retained the existing app-controlled 180-degree flip as a separate output transform;
- reduced normal orientation logging to initialization and actual rotation changes;
- kept detailed X/Y/Z sample diagnostics available behind a separate compile-time define;
- ESP32-C3 and classic ESP32 remain free of accelerometer code unless an `IDOTMATRIX_ACCEL_DRIVER_*` backend is selected.

## 0.6.0-dev / Build 163

First orientation-sensor development build.

- added compile-time accelerometer backend selection;
- selecting a supported `IDOTMATRIX_ACCEL_DRIVER_*` automatically enables the common orientation subsystem;
- added the MatrixPortal S3 LIS3DH backend at I2C address `0x19`;
- added normalized X/Y/Z sampling, dominant-axis classification, hysteresis and stable-direction timing;
- added Serial diagnostics for the four physical panel orientations;
- display rotation is intentionally not applied yet;
- ESP32-C3 and classic ESP32 targets remain free of accelerometer dependencies unless a driver is explicitly enabled.

The runtime behavior outside the new diagnostic orientation subsystem is unchanged from the 0.5.0 baseline.
