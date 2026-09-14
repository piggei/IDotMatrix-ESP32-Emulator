# BUILD 121 - v0.4.1-dev: iOS captured 32x32 identity A/B test

Controlled iOS compatibility experiment based on BUILD 120 diagnostics.

- Keeps the BUILD 120 BLE/GATT/CCCD diagnostics unchanged.
- Keeps the automatic Device Info push unchanged so only the model/advertising identity is intentionally varied.
- Selects iDotMatrix logical profile `0x03` (32x32).
- Enables logical-to-physical preview so the 32x32 framebuffer is downscaled onto the tester's physical 16x16 WS2812B matrix.
- Uses the complete manufacturer record captured from a real 32x32 iDotMatrix unit exactly as provided:
  `54 52 00 70 03 04 0F 00 01 04`.
- Does not claim that these bytes apply to the original 16x16 or 64x64 models.
- Purpose: determine whether the iOS app requires a more complete/recognized manufacturer identity before it begins sending normal FA02 application commands.
- No NimBLE migration and no speculative protocol workaround are included.

# BUILD 120 - v0.4.1-dev: iOS GATT/CCCD diagnostics and generic repository defaults

Diagnostic development build following the first external iOS/Android comparison from BUILD 119.

- BUILD 119 evidence showed that iOS establishes the BLE connection and receives the emulator's delayed Device Info push, but sends no FA02 or AE01 application writes before the app reports Clock error `10011` or GIF send error `10019`.
- Added FA03/AE02 characteristic READ tracing.
- Added FA03/AE02 CCCD (`0x2902`) READ/WRITE tracing and decoding of notification/indication enable bits.
- Added characteristic notification-status callbacks so the next trace can distinguish a successful notification from disabled/no-subscriber/error states.
- Expanded periodic/disconnect diagnostic counters for GATT reads, CCCD writes and notification status events.
- No speculative iOS workaround has been introduced; BUILD 120 remains evidence-gathering only.
- Changed public repository defaults to a generic configuration: WS2812B data GPIO4, OLED disabled, external status LED disabled and all buzzer outputs disabled.
- Changed `LITTLEFS_FORMAT_ON_MOUNT_FAIL` default to `1`: the firmware still attempts a non-destructive mount first, then formats only after mount failure. Users requiring strict forensic preservation can set it back to `0`.
- Active-buzzer support remains implemented but opt-in. Passive-buzzer hardware validation is planned separately.

# BUILD 119 - v0.4.1-dev: iOS error 100019 handshake diagnostics

Diagnostic-only development build based on v0.4.0 / BUILD 118.

- Added `IOS_HANDSHAKE_DIAG` tracing for an external report that the official iDotMatrix iOS app returns connection error `100019` while Android works.
- Logs BLE/GATT setup, advertising metadata, connect/disconnect events, FA02/AE01 writes, FA03 notifications, relative handshake timing, session counters and free heap.
- Large payload dumps are capped so initial handshake evidence remains readable.
- No iOS compatibility workaround is intentionally included yet; BUILD 119 is evidence-gathering only.
- Added `docs/IOS-100019-DIAGNOSTICS.md` with reproducible instructions for the external tester.

# BUILD 118 - v0.4.0 final release

Final packaging release based on the hardware-tested BUILD 117 release candidate.

- Changed the public release identifier from `0.4.0-dev` to `0.4.0`.
- Kept BUILD 117 runtime behavior unchanged.
- Declared classic ESP32 as the supported and validated v0.4.0 reference target.
- Documented ESP32-C3 as experimental/unsupported after direct testing showed FastLED channel/driver timeouts despite successful boot and substantial runtime initialization.
- Documented ESP32-S3 as a planned next-phase target for native HUB75 64x64 and PSRAM testing.
- Preserved the explicit distinction between protocol compatibility and intentional emulator UX improvements.
- Finalized release documentation and packaging.

# BUILD 117 - v0.4.0-dev: release-candidate consolidation

Documentation/source hygiene checkpoint before the planned v0.4.0 final release.

- No intended protocol or feature behavior changes from BUILD 116.
- Disabled verbose TEXT and Bulk protocol tracing by default while retaining the compile-time diagnostics.
- Fixed the OLED startup banner so it reports the current `FW_BUILD` instead of the stale hard-coded `B63`.
- Updated Device Info documentation to the hardware-validated BUILD 109 result: response bytes `00 04` are displayed by the official app as MCU `0.04`.
- Removed/qualified obsolete password-timing claims from current protocol documentation.
- Added `docs/ORIGINAL-HARDWARE-64X64.md` to separate direct original-device observations from intentional emulator UX differences and to prepare for physical teardown documentation.
- Consolidated the long-text and connection-feedback work from BUILD 111-116.
- Translated remaining active source comments to English and removed stale implementation-era labels where they no longer aid maintenance.

# BUILD 116 - v0.4.0-dev: decoupled TEXT motion and visual refresh

- Dynamic TEXT colors/effects can redraw at a faster visual cadence without advancing X/Y position.
- Scroll speed is governed only by the app speed setting, eliminating the previous solid-color versus Rainbow speed mismatch.

# BUILD 115 - v0.4.0-dev: continuous vertical TEXT promotion fix

- Corrected UP/DOWN page promotion so the current page travels a full `glyphHeight + 1` step before the queued page becomes current.
- Eliminated the premature blank/reset observed when text reached the center of the display.

# BUILD 114 - v0.4.0-dev: connection-buzzer compile hotfix

- Removed a stale duplicate `wanted` declaration in `updateBuzzer()`.
- No intended runtime behavior change from BUILD 113.

# BUILD 113 - v0.4.0-dev: continuous vertical TEXT and connection feedback

- UP/DOWN render the next text page immediately behind the current page with a 1 px separator, avoiding a fully blank interval.
- Added one low-priority non-blocking 90 ms buzzer pulse on BLE connection instead of reproducing the original hardware connection animation/logo.
- Connection feedback is skipped when a higher-priority buzzer event is active.

# BUILD 112 - v0.4.0-dev: paged viewport for all non-horizontal TEXT effects

- Extended long-text paging to Blink, Breathe, Snowflake and Laser.
- Page changes preserve effect phase; Snowflake/Laser do not restart when the visible glyph page changes.
- LEFT/RIGHT remain continuous full-line scrolling.

# BUILD 111 - v0.4.0-dev: long-text paging

- Added a resolution-independent text viewport based on matrix width and glyph advance.
- PIN advances through the complete phrase by pages.
- UP/DOWN advance through all pages instead of permanently clipping to the first visible glyphs.

# BUILD 110 - v0.4.0-dev: documentation consolidation

- Consolidated the hardware-tested app-facing version findings and kept release/build identifiers explicitly separate.
- No intended runtime change from BUILD 109 other than the internal build number.

# BUILD 109 - v0.4.0-dev: Device Info version field verified

- Changed the 9-byte Device Info FA03 response to encode `FW_RELEASE_MAJOR` and `FW_RELEASE_MINOR`.
- Hardware testing confirmed bytes `00 04` are displayed by the official app as MCU `0.04`.
- `FW_BUILD` remains an internal development identifier and is not sent in this two-byte field.

# BUILD 108 - v0.4.0-dev: advertising version experiment

- Tested release/profile bytes in BLE manufacturer/advertising data.
- Official-app testing showed this did not change the MCU version displayed in Device Information, leading to the BUILD 109 Device Info correction.

# BUILD 107 - v0.4.0-dev: clock layout fine tuning

- Fine-tuned clock effects 2, 4 and 5 after physical display comparison.
- Retained one-second time-colon blinking and unchanged date-slash behavior.

# BUILD 106 - v0.4.0-dev: clock separator direction correction

- Corrected separator movement directions after accounting for the flipped physical display orientation.
- No protocol or non-clock behavior changes.

# BUILD 105 - v0.4.0-dev: clock separator alignment and blinking

Aesthetic-only clock-rendering update. No protocol, storage, BLE, Carousel, Alarm, Schedule, Countdown, reset or other runtime behavior was intentionally changed.

- Clock effects 0, 3, 5, 6 and 7 move the time colon one pixel to the right.
- Clock effects 1 and 4 move the time colon one pixel to the left.
- Clock effect 2 keeps its existing separator position.
- The time colon now blinks once per second in all clock effects.
- The date separator `/` remains continuously visible and keeps its previous position.

# BUILD 104 - v0.4.0-dev: consolidation after password experiments

- Removed the experimental password SET/VERIFY runtime implementation introduced in BUILD 101-103. The official app remained on the Set Password screen with immediate, early and nominally deferred `05 00 04 02 01` acknowledgements, so the full SET transaction is not considered understood.
- Preserved the password protocol findings as documentation only: observed SET frame `04/02`, six-digit decimal-pair encoding, likely VERIFY frame `05/02`, app-side `pwdByMac` caching, and original-device reset clearing the stored password association.
- Retained BUILD 101's verified live-reset UX correction: `03/80` clears emulator-managed persistent state, leaves BLE connected, preserves the volatile synchronized software clock, keeps the logical display ON, and presents a black framebuffer ready for the next command.
- Disabled verbose Device Info tracing by default while keeping it available for future MCU-version reverse engineering.
- No intended changes to the hardware-tested Carousel, GIF, Alarm, Schedule, Countdown, Stopwatch, ECO, rotation or renderer behavior.

# BUILD 103

Password handshake timing experiment. BUILD 102 proved that sending `05 00 04 02 01` earlier does not release the official app password-setting screen. BUILD 103 attempted a 25 ms delayed SET/VERIFY notification based on original-hardware Android logcat timing. Subsequent source review found that the experiment was still serviced from the FA02 write-processing path rather than from a truly independent main-loop dispatcher, and hardware testing again left the app on the Set Password screen. BUILD 104 removes this experimental runtime path rather than preserving an unverified implementation.


## v0.4.0-dev — BUILD 102

Password SET handshake hotfix. A syntactically valid `04/02` SET command is now acknowledged on FA03 immediately with `05 00 04 02 01`, before Preferences/NVS persistence is attempted. This matches the timing expected by the official app and decouples protocol acknowledgement from local durability. Invalid password-pair payloads still receive status `0x00`. No password value is printed in diagnostics. Runtime behaviour outside the password SET path is unchanged from BUILD 101.

## BUILD 101 - Password handshake diagnostics and live reset

- Added persistent six-digit iDotMatrix password SET support for `04/02`, using the observed decimal-pair encoding (`123456` -> `0C 22 38`).
- Added VERIFY support for `05/02`, returning FA03 status `01` on match and `00` on mismatch.
- Added `PASSWORD_FORCE_VERIFY_FAIL` for controlled app-handshake experiments; no global BLE command enforcement is enabled yet.
- Password diagnostics report only enabled/valid/result state and never print the secret value.
- `03/80` now clears stored password state in addition to the emulator-managed persistent settings already reset in BUILD 99.
- Corrected live-reset UX: reset no longer simulates a power cycle while BLE remains connected. It preserves volatile synchronized time, leaves the matrix logically ON, clears it to black, and remains ready for the next app command.
- Added Device Info diagnostics while the encoding of original MCU version `5.11` in the 9-byte response is investigated; the response bytes themselves are unchanged in this build.

# BUILD 100 — v0.4.0-dev: compile hotfix

- Added the missing forward declaration for `carouselEnterRequested`, which is referenced by `stopAlarm()` before the Carousel state definitions later in the monolithic Arduino sketch.
- No runtime or protocol behavior changes relative to BUILD 99.

# BUILD 99 — v0.4.0-dev

Hardware-oracle consolidation and boot/reset policy.

- Added automatic boot policy: valid optional RTC starts Clock; otherwise a valid persisted Device Assets bank resumes automatically; otherwise the matrix stays off.
- Added persistent Carousel resume after power cycle, matching behavior observed on an original iDotMatrix 64×64.
- Changed Program/Schedule buzzer from a continuous/repeating request to a single three-beep one-shot at activity start. Alarm remains repeating. Countdown remains a one-shot emulator enhancement.
- Schedule and Alarm now restore a previously running Carousel after the event ends.
- Changed command `03/80` from runtime-only reset to destructive emulator reset: Carousel, Alarm, Schedule, brightness persistence, ECO, rotation and volatile time sync are cleared.
- Made Clock time reads RTC-aware so a valid optional RTC can drive Clock immediately at boot.
- Documented original 64×64 observations: no RTC persistence, volatile software clock after app sync, persistent boot Carousel, Alarm/Program buzzer behavior, silent original Countdown, Power Saving, 180-degree Flip, transient Cloud/Graffiti behavior, reset clearing Device Assets/password, and MCU version 5.11.
- Password BLE semantics and MCU-version query remain open protocol work.

## BUILD 98 - v0.4.0-dev: Countdown completion buzzer

- Added a local active-buzzer notification when Countdown reaches zero naturally.
- Reuses the existing non-blocking buzzer timing: three short 90 ms pulses separated by 70 ms gaps.
- Countdown completion is one-shot: after the third pulse the buzzer stops automatically instead of entering the repeating Alarm/Schedule trill cycle.
- Existing Alarm and Schedule buzzer behavior is intentionally unchanged.
- Countdown reset or a newly started Countdown cancels any completion trill still in progress.
- Soft/runtime reset also clears the one-shot buzzer request.
- The existing spontaneous Countdown completion status `05 00 08 80 03` is unchanged; the buzzer is emulator-side behavior and adds no BLE packet.

## BUILD 97 - v0.4.0-dev: Device Assets consolidation

- Consolidation checkpoint after successful hardware validation of BUILD 96.
- Confirmed the temporary matrix blackout works during Device Assets bank replacement and does not disturb subsequent carousel playback.
- Retains the hardware-tested BUILD 95/96 behavior: one 12-slot device bank, mixed GIF/TEXT slots, per-slot dwell, autonomous rotation, and continued playback after the official app disconnects.
- No carousel protocol, storage, renderer, BLE, timing, or blackout behavior is intentionally changed from BUILD 96.
- Disabled verbose `CAROUSEL_PROTOCOL_DEBUG` tracing by default; it remains available as a compile-time diagnostic option for future reverse engineering.
- Aligned README, PROTOCOL, HISTORY, TODO, and protocol-comparison wording with the verified status of the feature.
- Mixed GIF/TEXT playback is considered hardware-tested on the emulator; equivalent TEXT persistence/playback on original iDotMatrix hardware remains unverified.

## BUILD 96 - v0.4.0-dev: Device Assets upload blackout

- BUILD 95 hardware validation succeeded with short three-item pages, a full 12-position mixed GIF/TEXT page, repeated carousel cycles, and continued autonomous playback after the official app disconnected.
- During a Device Assets bank replacement, the physical matrix is now forced black from `02/01` until the stored bank is ready to start. This avoids displaying a frozen frame while the app uploads the replacement assets.
- The blackout is an emulator UX policy only: it does not modify the protocol-controlled `screenOn` state, framebuffer contents, brightness, or stored carousel metadata.
- `refreshMatrix()` honors the temporary upload blackout, so incidental renderer refreshes during the push cannot expose stale or partially updated content.
- When the existing 3-second post-upload settle condition closes the replacement, the blackout is always released. If Assets view is active, playback starts; otherwise the preserved framebuffer is restored.
- Normal display-mode changes, live GIF playback, event GIF playback, and runtime reset explicitly clear the temporary blackout.
- Hardware validation subsequently succeeded; BUILD 97 consolidates the same blackout behavior without runtime changes.

## BUILD 95 - v0.4.0-dev: Device Assets mixed-content/session fix

- Supersedes BUILD 94 after hardware logs showed two incorrect assumptions in the carousel prototype.
- Official-app captures show `0A/01` can establish Assets view **before** a later page push and is not necessarily repeated after the Bulk transfers. BUILD 95 preserves this view intent across `02/01` instead of waiting forever for a second `0A/01`.
- A full 12-position capture exposed a `DataType.TEXT` Bulk between GIF `imageIndex=4` and GIF `imageIndex=6`. In BUILD 94 the normal TEXT renderer called `switchDisplayMode(DISPLAY_TEXT)`, which cleared `carouselUploadOpen`; GIF indices 6..11 were then handled as live GIFs.
- Carousel slots now persist a content type. GIF (`type=1`) and project-observed TEXT (`type=3`) assets with `imageIndex=0..11` can be stored in the 12-slot bank.
- TEXT carousel payloads are stored on LittleFS and rendered through the existing TEXT engine without invoking the normal display-mode teardown that would destroy carousel state.
- Playback remains suspended while replacement is active. Because current short-page captures contain no explicit end-of-push command, an already-requested Assets view resumes after a 3-second no-Bulk/no-new-asset settle interval. This interval is emulator policy, not an original-device timing claim.
- Added diagnostics for `timeSign` and `imageIndex` on TEXT Bulk transfers so the next hardware run can verify that the observed TEXT corresponds to slot 5.
- BUILD 94 remains useful evidence for the command ordering but is superseded for carousel validation.

## BUILD 94 - v0.4.0-dev: Device Assets upload/playback boundary fix

- Supersedes BUILD 93 after hardware testing showed that a longer 12-item page could begin playing newly received assets while the page was still being uploaded.
- Root cause: BUILD 93 preserved Assets-view intent across `02/01` and re-armed playback after each successful slot commit. This was useful for permissive ordering but exposed a partially replaced bank during long uploads.
- `02/01` now stops/suspends carousel playback and opens a replacement-only upload phase. Newly committed slots remain stored but are never started from the Bulk completion path.
- `0A/01` now closes the upload context and acts as the explicit Device Assets enter/start boundary.
- Repeated `0A/01` while the carousel is already active is idempotent instead of restarting from slot 0.
- No timing heuristic is used to decide when a page is complete.
- This sequencing matches independent hardware-validated public reverse engineering that documents material wipe/setup before a page push and `0A/01` as the Assets-view start command.
- Hardware validation pending: short pages, a full 12-slot page, dwell rotation, and BLE-disconnect autonomy.

# History

## BUILD 93 - v0.4.0-dev: 12-slot Device Assets protocol correction

- Supersedes BUILD 92 after app-side clarification that the UI contains three separate pages of 12 positions; only one page is pushed at a time, so the device bank is 12 slots rather than 36.
- Reinterpreted `02/01` as a slot-setup descriptor: byte 4 is the number of slot IDs and the following bytes identify device slots. The official app currently sends all `0..11` even when only a subset receives GIF media.
- Uses Bulk `imageIndex` directly as Device Assets slot `0..11` instead of allocating local slots by arrival order.
- Uses Bulk `timeSign` as per-slot dwell; project logs directly confirmed 5-second and 30-second values for slots 0, 1 and 2.
- Removed BUILD 92's 2-second upload-idle finalization heuristic. This heuristic could terminate a longer upload mid-page and cause later GIFs to fall back to the live RX -> PLAY path.
- `0A/01` is now treated as the explicit Device Assets view/start command and is tolerated before, during or after asset transfer. A page refresh while Assets view is already active can resume local playback once a valid slot has been stored.
- Empty configured positions are skipped by playback, allowing the official app to set up all 12 slots while uploading only the occupied subset.
- BUILD 92's temporary `/car12..35` files/NVS entries are cleaned up at boot.
- External hardware-validated reverse engineering independently corroborates a 12-slot autonomous carousel, `timeSign`, `imageIndex`, and `0A/01`; disconnect persistence remains to be verified on the emulator hardware.
- Hardware validation status: **pending**.

## BUILD 92 - v0.4.0-dev: Variable-length Device Assets carousel correction

- Supersedes BUILD 91 before hardware validation after a new official-app capture with **3 selected images** showed the exact same `11 00 02 01 0C 00 01 ... 0B` frame previously seen with 12 selected images.
- Corrected the interpretation of `0x0C`: it is no longer treated as carousel length, and the following 12 values are no longer treated as the selected-image list. Their exact protocol role remains open.
- Raised emulator carousel capacity to the app-observed maximum of **36 images**.
- Playlist length/order is now derived from successfully received GIF transfers during the `02/01` upload session. Local storage slots are allocated in receive order rather than from the candidate protocol `imageIndex`.
- A GIF is classified as a carousel asset only while a `02/01` carousel upload session is open. Normal live/Cloud GIFs therefore keep the stable v0.3.1 RX -> PLAY path regardless of their header index value.
- Kept bytes 13..14 (`timeSign` candidate) and byte 15 (`imageIndex` candidate) in diagnostics, but no longer rely on `imageIndex` for local slot identity until captures above 12 items clarify its semantics.
- Kept optional `0A/01` support, but removed it as a requirement because the supplied current-app captures show no such command after the upload.
- Added a 2-second post-upload idle heuristic to finalize a variable-length batch when no explicit end/count is observed. This timeout is emulator behavior, not a protocol claim.
- BUILD 91 is retained in history as the first prototype but should not be used for validation.
- Hardware validation status: **pending**.

## BUILD 91 - v0.4.0-dev: Device Assets carousel (superseded prototype)

- First development build after the stable v0.3.1 / BUILD 90 baseline.
- Added experimental handling of the official app Device Assets slot-setup command `02/01`; the observed 12-slot frame is `11 00 02 01 0C 00 01 02 03 04 05 06 07 08 09 0A 0B`.
- Cross-validated the previously ignored GIF Bulk header fields against independent original-hardware reverse engineering: bytes 13-14 are `timeSign` (little-endian dwell seconds) and byte 15 is `imageIndex`.
- GIF uploads with `imageIndex` 0-11 are now stored as persistent LittleFS carousel slots instead of being promoted to the live GIF PLAY file.
- Added per-slot Preferences metadata for dwell, size and CRC plus temp/backup/rollback storage protection.
- Added `0A/01` handling to enter/start the local carousel; slot GIFs loop for their dwell time and advance using a fresh AnimatedGIF decoder.
- A slot-setup upload may be followed by a start request before all GIFs arrive; playback is deferred until the declared slot set is complete.
- Normal live/preview GIF transfers remain on the v0.3.1 alternating RX -> PLAY path.
- Added Bulk diagnostics for `timeSign` and `imageIndex`, specifically to verify the user's 30-second and 60-second captures.
- Hardware validation status: **not performed; superseded by BUILD 92 before validation**.

## BUILD 90 - v0.3.1 stable release

- Final public release build for the v0.3.1 consolidation cycle.
- Added the explicit source-level `FW_RELEASE "0.3.1"` identifier alongside the internal `FW_BUILD 90`.
- No protocol, renderer, filesystem, BLE, media, Alarm/Schedule, buzzer, or runtime behavior was intentionally changed from BUILD 89.
- Hardware validation of BUILD 89 confirmed normal GIF/cloud playback, multi-packet transfers, Programs/Schedule, Alarm, isolated `/event_play.gif` playback, and the active buzzer.
- Completed the final code/documentation cross-check and promoted v0.3.1 from development status to the stable release.

## BUILD 89 - v0.3.1 consolidation: Alarm/Schedule GIF LittleFS playback

- Hardware validation after completion confirmed GIF/cloud playback, Programs/Schedule, Alarm, isolated event-GIF playback, and the active buzzer operate correctly on the reference hardware.
- Hardware validation also confirmed BUILD 88 fixed the false multi-packet Bulk timeout regression.
- Alarm and Schedule GIF playback no longer allocates the compressed GIF as one contiguous DRAM buffer.
- Added a dedicated disposable `/event_play.gif` path. Persistent Alarm/Schedule media is copied to this path and CRC-checked before AnimatedGIF opens it.
- The persistent Alarm/Schedule source files are therefore never held open by the decoder and remain safe to rename during later staging/backup/rollback transactions.
- Event GIF playback uses the same LittleFS file callbacks and fresh AnimatedGIF decoder lifecycle as normal GIF playback.
- Removed the now-unused `gifData`/`allocateGIF()` full-RAM path and its `MAX_GIF_SIZE` limit. Alarm/Schedule media remains subject to the independent 8192-byte reconstructed-packet transport ceiling.
- `/event_play.gif` is removed when event playback stops and at boot because it is disposable, not persistent state.

## BUILD 88 - v0.3.1 consolidation: Bulk timeout mutex hotfix

- Hardware log from BUILD 87 exposed a deterministic false Bulk timeout on multi-packet transfers (for example a 4189-byte GIF sent as 4096 + 93 bytes).
- Root cause originated in BUILD 86: `loop()` captured `millis()` before waiting for the runtime-state mutex. While it waited, the BLE callback could update `bulkLastRxMs` / `packetLastRxMs` to a newer timestamp. Unsigned elapsed-time subtraction then wrapped and looked like a huge timeout.
- BUILD 88 now captures the loop time snapshot only after the mutex has been acquired, so timeout evaluation and the protected receive timestamps use a consistent ordering.
- Single-packet transfers were unaffected because the Bulk transaction completed before the loop could evaluate an active transfer.
- No BLE framing, ACK value, timeout duration, filesystem policy, renderer or media format was changed.

## BUILD 87 - v0.3.1 consolidation: OTA and LittleFS safety

- Hardware validation status: not yet tested; BUILD 86 was confirmed by the user to compile, flash and operate normally.
- ArduinoOTA remains disabled by default. When enabled, compile-time assertions now reject the repository Wi-Fi placeholders, the default development OTA password, empty credentials and OTA passwords shorter than eight characters.
- Replaced implicit `LittleFS.begin(true)` formatting with a non-formatting mount by default. A mount error no longer silently erases stored media.
- Added explicit `LITTLEFS_FORMAT_ON_MOUNT_FAIL`, disabled by default, for intentional first-use/recovery formatting only.
- Added a `littleFsReady` runtime state so recovery, playback and media-write paths fail safely when storage is unavailable while Preferences metadata can still be loaded.
- Normal Bulk GIF reception now checks current LittleFS free space before opening the RX file and terminates the transfer if the declared GIF cannot fit.
- Alarm/Schedule GIF streaming migration was deliberately deferred: directly opening their transactional source files would race with rename/backup replacement. A future build should use an isolated event PLAY file/path instead.
- No observed BLE packet framing, successful-command ACK, renderer output or protocol field interpretation was intentionally changed.

## BUILD 86 - v0.3.1 consolidation: cross-core synchronization

- Added a FreeRTOS task mutex that serializes shared protocol/runtime state between FA02/server BLE callbacks and the Arduino `loop()` task.
- Removed `volatile` from the deferred GIF and packet/Bulk timeout fields now covered by the mutex; `volatile` is no longer used as a substitute for cross-core synchronization on those paths.
- Packet/Bulk timeout cleanup, normal command processing, renderer/runtime updates and deferred GIF promotion/open now cannot mutate the same shared state concurrently.
- Removed the blocking `delay(300)` from the BLE disconnect callback. Advertising restart is deferred to `loop()` after the same 300 ms interval.
- The mutex is a scheduler/task mutex, not a critical-section spinlock, so filesystem/decoder/rendering operations never run with interrupts disabled.
- Full FA02 parsing and filesystem/Bulk work still execute from the BLE callback and remain technical debt for a future architectural cleanup.
- No observed packet framing, command payload or ACK value was intentionally changed.

## BUILD 85 - v0.3.1 consolidation: runtime correctness

- Hardware test confirmed successful compilation, flashing and normal operation of the BUILD 85 runtime changes.
- ECO/power-saving output now re-evaluates effective brightness once per second so static display modes react when configured time boundaries are crossed.
- Added sanity validation for incoming power-saving hour/minute/reduction fields; malformed records are ignored while the existing compatibility ACK is preserved because original-device error semantics remain unknown.
- Runtime soft reset now clears transient renderer state, active Alarm/Schedule runtime markers, timer state and deferred GIF-open state instead of leaving stale flags that could resurrect content later. Persistent Alarm/Schedule configuration, time sync, brightness, ECO settings and screen power remain intact.
- When optional RTC support is enabled, `rtc.lostPower()` now marks RTC time invalid. Alarm, Schedule and ECO only consume RTC time after it is valid; a successful BLE time sync marks the adjusted RTC valid again.
- No successful-command packet framing or known ACK value was intentionally changed. ECO polling and soft-reset state policy remain emulator implementation behavior, not claims about original hardware.

## BUILD 84 - v0.3.1 consolidation: Arduino compile hotfix

- Hardware test after the hotfix confirmed successful compilation, flashing and normal operation of the BUILD 83 hardening path.
- Fixed Arduino `.ino` preprocessing compatibility introduced by BUILD 83.
- Added an explicit `AlarmSlot` forward declaration so the generated prototype for `saveAlarmMetaValue()` sees the type.
- Added an explicit `clearFramebuffer()` declaration before Alarm code and moved its default argument to the declaration.
- No protocol, persistence, renderer or runtime behavior changes relative to BUILD 83.

## BUILD 83 - v0.3.1 consolidation: transactional state/media hardening

- Alarm updates are staged and validated before the live slot is modified. Full-media updates use temporary + backup files and only publish the new in-memory metadata after media replacement and NVS persistence succeed.
- Added best-effort Alarm rollback on filesystem/NVS failure and startup reconciliation of `.bin`/`.bak` media against persisted size/CRC metadata.
- Schedule global flags are now staged during an upload instead of being published before the activity list commits.
- Schedule commit preflights every received temporary media file by size/CRC, backs up the previous media set, promotes the new set, persists metadata, and publishes runtime state only after those steps succeed.
- Added best-effort Schedule filesystem/preferences rollback. Startup recovery reconciles destination/backup files against persisted activity metadata after an interrupted replacement.
- Normal GIF RX -> PLAY promotion now preserves the previous PLAY file until the new file has been renamed successfully; reboot recovery restores a lone PLAY backup and discards non-resumable RX remnants.
- Fixed Alarm/Schedule preemption ordering: an active Schedule is stopped/restored before Alarm media starts, so Schedule teardown cannot immediately stop the Alarm GIF.
- Alarm restores SOLID/RAW/GRAFFITI framebuffer-backed modes after completion; other dynamic modes still fall back to clock/blank because their prior runtime state is not generically reconstructible.
- Added hour/minute sanity validation for Alarm and Schedule configurations.
- No successful-command packet framing or experimentally established ACK value was intentionally changed. Transaction/timeout policies remain emulator implementation behavior, not original-device protocol claims.

## BUILD 82 - v0.3.1 consolidation: parser and transfer hardening

- Added calendar validation for BLE time synchronization before updating the software clock or optional RTC.
- Preserved the existing time-sync ACK on malformed input because negative/error ACK semantics remain experimentally unknown.
- Added a conservative 5-second inactivity timeout for incomplete logical-packet reassembly.
- Added a conservative 30-second inactivity timeout for active Bulk transfers.
- Added cleanup of partial GIF RX files when transfers are aborted, time out, overflow, or the BLE connection closes.
- Rejects declared TEXT Bulk payloads larger than `MAX_TEXT_PAYLOAD` (4096 bytes) instead of silently retaining only a prefix.
- Timeout values are emulator safety guards and are not claimed as observed original-device protocol timings.
- No known command framing, successful-transfer ACK behavior, rendering path, or media format was intentionally changed.

## BUILD 81 - v0.3.1 consolidation: documentation and hygiene

- First development build toward `v0.3.1`; public stable baseline remains `v0.3.0 / BUILD 80`.
- No intentional protocol or runtime behavior changes; only the internal build identifier is incremented.
- Corrected stale TEXT documentation so the canonical records are `4-byte metadata + bitmap` (20 bytes for marker `0x02`, 68 bytes for marker `0x05`).
- Removed the obsolete `7 META + 13 BITMAP` example and related open-question wording.
- Clarified that Alarm playback currently implements GIF and RAW media; TEXT has been observed in captures but is not implemented by `loadAlarmMedia()`.
- Clarified Schedule staging/commit behavior: the emulator commits after an inactivity timeout, but the current filesystem replacement is not guaranteed to be atomic.
- Clarified that Schedule restore-to-previous-mode is only implemented for selected framebuffer-backed modes, with fallback to clock/none for other modes.
- Documented the 8192-byte logical packet ceiling that also constrains embedded Alarm/Schedule media, independently of legacy GIF/RAM limits.
- Corrected the local brightness comment to `50/255` and translated the remaining Italian Alarm header label in `PROTOCOL.md`.
- Updated the WLED TODO: WLED integration is now a separate project rather than a pending module of this emulator.
- Added explicit release/build mapping and security considerations to the README.


## BUILD 80 - clock/date pixel alignment

- Final micro-positioning pass for date rendering in clock effects 1, 4, 6, 7 and 8.
- Shifted the affected date digits/clock separator geometry two pixels to the right while preserving the date slash position.
- No protocol, BLE, GIF, Alarm, Schedule, countdown or stopwatch behavior changed.

## BUILD 79 - date slash with clock effects preserved

- Date display keeps the selected clock visual effect instead of switching to a separate plain renderer.
- `HH:MM` uses the normal colon; the 5-second date phase renders `DD/MM` with a slash.

## BUILD 77-78 - native 16x16 and timer UI refinement

- Restored the native 16x16 (`0x01`) profile after the 64x64 experiments; logical-to-physical preview remains optional and disabled by default.
- Added the matching vertical stopwatch/countdown design: animated timer icon above, time below.
- Stopwatch advances forward and stays white.
- Countdown remains white until the final five seconds, then turns red.
- Added optional clock date cycle: 30 seconds of `HH:MM`, then 5 seconds of `DD/MM`, using the clock renderer/effect selected by the app.

## BUILD 76 - first clock/date and countdown UI pass

- Added the first vertical countdown timer layout and the 30 s / 5 s clock/date cycle.
- This build was subsequently refined in B77-B80.

## Builds 68-75 - 64x64 investigation

- `0x04` successfully makes the official app expose 64x64 content.
- Confirmed 4096-byte bulk chunks and existing ACK semantics at 64x64.
- B68 exposed static DRAM pressure from 64x64 framebuffers.
- B69 moved logical framebuffers to runtime allocation.
- B70 introduced LittleFS storage for large GIF payloads.
- B71 moved GIF startup out of the BLE callback.
- B72 isolated receive and playback files.
- B73/B74 investigated AnimatedGIF lifecycle corruption.
- B75 established the stable solution: fresh decoder instance per GIF; repeated small/large media switches no longer corrupt heap.

## Documentation update after Build 62

- Added a real hardware photograph to the README and `assets/`.
- Added `docs/PROTOCOL-COMPARISON.md` to cross-check independently derived findings against public client implementations and original-hardware research.
- Documented the distinction between client/controller implementations and this project's BLE peripheral/server emulation approach.
- Added independently corroborated ACK semantics (`0x01` intermediate/continue, `0x03` complete).
- Added the multi-size 16x16 / 32x32 / 64x64 research roadmap and request for testers.
- No firmware behavior changed; Build 62 remains the stable firmware baseline.


Development history of the ESP32 iDotMatrix emulator.

> Early builds were produced very iteratively during reverse engineering, and a reliable per-build history is not available for every number. Where the exact build number is not documented, this file describes the development period without inventing an attribution.

## BUILD 62 - Pure event-driven OLED

### Fixed
- Removed the remaining matrix animation stalls caused by periodic SSD1306 framebuffer transfers over software I2C.
- Removed all timed OLED refreshes during normal operation.

### Changed
- OLED is refreshed only after meaningful events: BLE, power, mode, brightness, Schedule/Alarm, timer state and unknown commands.
- Stopwatch/countdown show their state at the event without continuously redrawing elapsed time.

### Status
- Verified on hardware: matrix animations remain smooth with the OLED enabled.

## BUILD 61 - Event-driven OLED + slow refresh

### Fixed
- First mitigation for the performance regression introduced by the B60 OLED dashboard.

### Changed
- Normal OLED refresh reduced to about 2 seconds.
- Immediate refresh on major events.
- Countdown/stopwatch limited to about one refresh per second.
- Removed uptime from the dashboard.

### Known issues
- Periodic full-frame SSD1306 updates through U8g2 software I2C still caused visible matrix stalls. Fixed in B62.

## BUILD 60 - Consolidated baseline

### Added
- First consolidated baseline intended for the repository.
- FA02 unhandled-command monitor directly on the OLED.
- OLED alert with packet length, counter and first 12 RAW bytes.
- `UNK` counter for unknown commands.

### Changed
- Hardware configuration consolidated for the DollaTek ESP32 OLED board.
- Configurable defines collected and cleaned up.
- Experimental PNG diagnostics reduced after decoder stabilization.

### Known issues
- OLED dashboard redrawn every 250 ms with `F_SW_I2C`, severely slowing matrix animations. Mitigated in B61 and fixed in B62.

## BUILD 59 - Live OLED dashboard

### Added
- OLED dashboard with BLE, screen state, mode, time, brightness, Schedule, heap and stack.
- Correct OLED rotation for the physical board orientation.

### Changed
- Larger, more readable OLED font.

## BUILD 58 - Exact OLED test

### Fixed
- OLED initialization aligned with a U8g2 sketch already verified on the actual board.
- `begin`, power-save and contrast sequence verified with a startup splash.

## BUILD 57 - OLED migration to U8g2

### Changed
- Removed Adafruit GFX/SSD1306 from OLED diagnostics.
- Switched to U8g2 software I2C with SCL=15, SDA=4, RESET=16.

## BUILD 56 - OLED compilation fix

### Fixed
- Fixed Arduino preprocessor conflict involving `DisplayMode` in the OLED diagnostic helper.

## BUILD 55 - First OLED diagnostics

### Added
- Initial onboard OLED support, originally using Adafruit libraries.
- `OLED_STATUS_ENABLED` compile-time switch.

## BUILD 54 - Timer protocol sniffer

### Diagnostics
- RAW FA02 fragment logging plus reconstructed logical FA02 packet logging.
- Focused stopwatch/countdown diagnostics.

### Protocol
- Confirmed that the app does not periodically poll timers.
- Confirmed stopwatch reset/start/pause/resume states.
- Confirmed countdown states and spontaneous end-of-countdown notification.

## BUILD 53 and earlier - Schedule/PNG stabilization

### Fixed
- Stabilized Schedule PNG loading and rendering.
- Identified and fixed stack overflow during PNG inflate by moving heavy state/buffers away from the loop-task stack.
- Stabilized handling of multiple activities and restoration of supported framebuffer-backed previous modes; other modes fall back to clock/none in the consolidated firmware.

### Protocol
- Discovered that Schedule activity ACK must end with status `0x03`; `0x01` makes the app stop sending and report an error.
- Implemented sequential reception of programs with many activities.

## Alarm / Schedule development period

### Added
- 10 persistent Alarm slots in flash/LittleFS.
- Alarm-associated media storage/playback for GIF and RAW images. TEXT content has also been observed in Alarm captures, but Alarm TEXT playback is not implemented in the consolidated firmware.
- Weekdays, duration, enable and buzzer flags.
- Persistent Programs/Schedules with timed activities.
- Schedule support for GIF, text and PNG.
- Up to 32 Schedule activities in the consolidated firmware.

### Protocol
- Reverse engineered global Schedule command `07 80` and activity command `05 80`.
- Confirmed that the app edits inactive programs locally and transfers their activities when the program is activated.

## BUILD 32 - Audio protocol diagnostics

### Diagnostics
- Build identified in logs as `B32-audio-protocol-diagnostics`.
- Separate logging for LEVEL and FFT packets.

### Protocol
- Identified 5 LEVEL modes and 5 FFT modes used by the app.
- Confirmed level/sensitivity data in LEVEL traffic.
- Confirmed FFT band structure and related ACKs.

## Audio-effects development period

### Added
- Implemented the 10 audio visualizations observed in the app.
- Five LEVEL effects: dancer, heart, dotted-frame spectrum and two animated faces.
- Five FFT effects: symmetric bars and color variants, reactive heart, horizontal/vertical spectra.

### Changed
- Corrected mode-index mapping after systematic comparison with an app video.
- Refined individual graphics based on the observed reference behavior.

## GIF / Bulk / Text / Effects development period

### Added
- Reconstruction of BLE-fragmented FA02 packets.
- Bulk transfers with size, CRC32 and intermediate/final ACKs.
- 16x16 GIF playback through AnimatedGIF.
- Text rendering from app-provided bitmap/glyph payloads.
- Graffiti/DIY and solid color.
- Configurable visual effects with speed and palette.
- Clock styles, scoreboard, brightness, rotation and power saving.

### Fixed
- Multiple GIF fixes: transparency, disposal, frame timing and restart behavior.
- Progressive memory reductions to preserve enough heap for BLE and FastLED.

## Early builds

### Added
- BLE advertising compatible with the iDotMatrix app.
- FA and AE services/characteristics.
- 16x16 device info.
- Time synchronization from the app.
- Matrix ON/OFF control.
- Initial FA03 ACK implementation.

### Note
- The exact numbering/history of the earliest builds was not preserved reliably enough to create a per-build changelog without speculation.
