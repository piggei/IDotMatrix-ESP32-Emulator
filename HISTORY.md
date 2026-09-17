# BUILD 144 - iOS device-originated RCSP stimulus probe

- Preserves the BUILD 143 passive RCSP diagnostics and the BUILD 142 delayed FA03 Device Info notification.
- Adds one deterministic, unsolicited raw JieLi-style authentication challenge (`0x00` + 16 bytes) on AE02 900 ms after both FA03 and AE02 notifications are enabled.
- The stimulus is explicitly diagnostic only: it does not claim to reproduce stock iDotMatrix ordering, does not verify cryptography, and does not forge authentication success.
- Any AE01 `0x01 + 16-byte` response is logged as evidence that the iOS JieLi authentication layer is actively listening to AE02.
- Keeps opcode `0x03 GET_TARGET_FEATURE` diagnostic-only; no unverified MCU/target-feature payload is generated.
- Disconnect summary now reports whether the RCSP stimulus was actually transmitted.

# BUILD 143 - iOS RCSP probe

- Preserves the BUILD 142 32x32 iOS identity and delayed FA03 Device Info experiment.
- Adds complete AE01 raw-write logging and FA03/AE02 characteristic-read diagnostics.
- Classifies JieLi raw authentication messages and `FE DC BA ... EF` RCSP frames.
- Adds a conservative response only for RCSP opcode `0x06` (session/auth reset), echoing the observed sequence in a generic success ACK.
- Detects opcode `0x03` (`GET_TARGET_FEATURE`) but does not fabricate the unknown iDotMatrix-specific target payload.
- Extends disconnect diagnostics with RCSP/auth/read/TX counters.

# BUILD 142 - v0.5.0-dev: iOS delayed Device Info handshake experiment

- Branched directly from the hardware-validated BUILD 141 consolidation baseline; no Preset, Schedule, Alarm, Carousel, MTU or media-path behavior is intentionally changed.
- Added a dedicated PlatformIO environment, `ios_dev_esp32_ws2812_32`, for Thiago's classic ESP32 test hardware: physical 16x16 WS2812 on GPIO17 while exposing logical iDotMatrix profile `0x03` (32x32).
- Reintroduced iOS diagnostics in an isolated compile-time path (`IOS_HANDSHAKE_EXPERIMENT=1`) rather than merging the old BUILD 119-122 experimental branch back into the normal MatrixPortal target.
- Preserved the exact 10-byte manufacturer payload captured from a real 32x32 device: `54 52 00 70 03 04 0F 00 01 04`.
- Changed the unsolicited Device Info experiment: BUILD 142 does **not** send Device Info immediately after connection. It waits until the client has enabled notifications on both FA03 and AE02, then schedules exactly one FA03 Device Info notification 250 ms later.
- The experimental Device Info payload is `09 00 01 80 04 0E 01 03 00`. Bytes `04 0E` come from the documented original-device 16x16 capture; only the profile byte is changed to `0x03`. This is an explicit hypothesis for the iOS test and must not be documented as a captured real-32x32 Device Info response.
- Added narrow `[IOSDIAG]` logging for CCCD writes, delayed Device Info scheduling/transmission, first application writes on FA02/AE01 and disconnect summary counters.
- BUILD 122 established the baseline: iOS connected and subscribed to FA03/AE02 for ~71 s but sent no FA02/AE01 traffic while unsolicited Device Info was suppressed. The same emulator accepted a manual FA02 Clock command from LightBlue, proving the GATT write path and application command handler work independently of the official iOS app.
- The normal `matrixportal_s3_hub75_64` environment remains available and keeps the BUILD 141 behavior because the iOS experiment is compile-time isolated.

# BUILD 141 - v0.5.0-dev: Preset/Default consolidation baseline

- Consolidation checkpoint after successful hardware validation of BUILD 140 Preset/Default playback. No intentional protocol or playback behavior changes.
- Disabled verbose `PRESET_PROTOCOL_DEBUG` tracing by default while retaining the compile-time diagnostic switch for future captures.
- Hardware validation confirmed mixed `TEXT -> GIF -> TEXT` playback, five-image ordered playback, 48,456-byte and 66,361-byte multi-packet Preset objects, and clean replacement of an already-running Preset bank.
- Consolidated Preset timing documentation: GIF/image items use the directly observed ~3-second dwell; LEFT/RIGHT scrolling TEXT advances only after the final glyph exits the display; PIN/page/viewport modes show all required pages and keep the final view for approximately 3 seconds.
- Kept Preset slots `14..19` volatile and separate from the persistent Device Assets Carousel. `timeSign=5` remains captured protocol metadata and is not interpreted as seconds.
- Corrected stale Schedule documentation: BUILD 137 hardware validation proved the large multi-packet `05/80` continuation model and ACK behavior.
- Removed a duplicate out-of-order BUILD 140 history block and aligned README, PROTOCOL, TODO and capture notes with the validated state.

# BUILD 140 - v0.5.0-dev: hardware-validated Preset / Default volatile playback

- Implemented the official app's Preset/Default media bank using captured device slots `0x0E..0x13` (14..19), separate from the persistent 12-slot Device Assets Carousel.
- Added mixed GIF/image (`type=1`) and TEXT (`type=3`) staging to volatile LittleFS files. Preset uploads no longer fall through to the ordinary live-preview GIF/TEXT path.
- Implemented `06/02 <count> <slots...>` activation for up to six unique slots in the app-supplied order.
- Direct hardware captures confirmed `timeSign=5` for all tested Preset media, including TEXT, small images and multi-packet 48,456-byte / 66,361-byte images. The field remains opaque metadata: GIF/image items use the observed ~3000 ms dwell, while TEXT timing follows the presentation mode (continuous scroll until the final glyph exits; page/static modes through the final page plus ~3000 ms).
- Preset files and metadata are deliberately volatile: they are removed on reboot/reset and are not written to Carousel Preferences or restored by the boot policy.
- A new Preset upload freezes any currently active Preset before replacement files are committed, preventing old/new banks from mixing during long transfers.
- Hardware validation confirmed mixed TEXT/GIF playback, a five-image sequence, large multi-packet assets and clean replacement of an active Preset.
- Preserved BUILD 137 hardware-validated Program/Schedule behavior and BUILD 138 targeted Preset diagnostics for the first implementation test.

# BUILD 139 - v0.5.0-dev: first Preset/Default playback implementation (non-compiling)

- Added the first runtime implementation of the volatile slots `14..19` bank and `06/02 <count> <slots...>` activation path.
- Introduced mixed GIF/TEXT Preset storage and ~3-second image playback based on the captured protocol.
- This build is **not a valid hardware baseline**: compilation failed because earlier Alarm code referenced `presetActive` before its declaration. BUILD 140 fixed the declaration order and added the hardware-observed content-aware TEXT timing.

# BUILD 138 - v0.5.0-dev: Preset / Default protocol diagnostics

- Added narrow diagnostics for the official app's Preset/Default section without implementing the feature yet.
- Bulk GIF/TEXT objects addressed to indices `0x0E..0x13` (14..19) now log `dataType`, `timeSign`, `imageIndex`, total size, CRC and the complete 16-byte Bulk header.
- Preset TEXT transfers additionally log the 14-byte TEXT global header and first glyph metadata record after CRC validation.
- Observed `06/02 <count> <slots...>` activation commands are decoded to a `PRESET ACT` diagnostic but deliberately remain unhandled so BUILD 138 cannot accidentally introduce playback semantics before the wire format is fully characterized.
- BUILD 137 Schedule behavior, Alarm handling, BLE framing, storage and updater behavior are unchanged.

# BUILD 137 - v0.5.0-dev: Schedule marker compile-scope hotfix

- Fixed a C++ scope error introduced in BUILD 136: `chunkMarker` was declared inside the first validation block but referenced by the following Schedule diagnostic block.
- The marker is now declared at packet scope, preserving the BUILD 136 Schedule protocol logic unchanged.
- No BLE, Alarm, ACK, timeout, filesystem, or Schedule transfer semantics were changed in this build.

# BUILD 136 - v0.5.0-dev: decode Schedule chunk marker + split updater build/upload

- BUILD 135 hardware logs showed the apparent Schedule media type changing from decimal `1` to `513`, exactly `0x0001 -> 0x0201`, while size, CRC, activity index, time window and media ID stayed unchanged.
- This proves the Schedule activity header does **not** contain a 16-bit `contentType` at offsets 10..11. Offset 10 is the one-byte media type; offset 11 is separate per-chunk metadata, observed as `0x00` on the first packet and `0x02` on continuation packets. Its broader semantics remain intentionally undocumented.
- The parser now reads `contentType` from offset 10 only and logs offset 11 separately as `m=0x..`. Because the marker is no longer part of media identity, the second chunk no longer triggers `SCH OBJECT CHANGE` and no longer discards the first 4096 bytes.
- Preserved the validated large-Schedule flow control: incomplete chunks receive ACK `0x01`; the CRC-valid final chunk receives ACK `0x03`. Alarm handling is unchanged.
- `update_idotmatrix_emulator.sh` now separates compilation from upload. After a compile-only `pio run`, the script verifies the firmware signature, clears the cached serial selection, performs a second mandatory MatrixPortal serial check so WSL/usbip can be re-bound/re-attached if the USB mode changed, and only then runs the upload target.

# BUILD 135 - v0.5.0-dev: preserve first large Schedule chunk across header variants

- BUILD 134 hardware logs confirmed that intermediate Schedule ACK `0x01` is the correct continuation request for a large 64x64 `05/80` activity: the app transmitted the full declared 48,456 media bytes instead of stopping after the first chunk.
- The same capture exposed a second issue. The serial totals advanced `4096 -> 4096 -> 8192 ... -> 44360/48456`, while the payload bytes sent by the app summed exactly to 48,456. This proves that the emulator discarded the first 4096-byte chunk when the second activity header differed in one of the fields used for transaction matching.
- Continuation identity now uses the immutable media tuple `contentType + mediaSize + mediaCRC` for the already-selected activity index. Schedule timing/day fields, `reserved` and `mediaId` remain activity/header metadata but no longer cause an in-progress media object to be reset when the media identity is unchanged.
- Added narrow `SCH HDR`, `SCH META VAR` and `SCH OBJECT CHANGE` diagnostics. They expose which ancillary header fields vary between chunks without enabling global FA02/RXASM packet spam.
- Added `SCH START` / `SCH START FAIL` runtime diagnostics so upload/commit can be distinguished cleanly from timed execution/media-decoder failures.
- The first chunk's activity metadata is retained for the committed Schedule. Later ancillary variants are diagnostic until their exact protocol meaning is characterized.
- Preserved BUILD 134 ACK behavior: incomplete large activity chunks return `0x01`; the final CRC-valid chunk returns `0x03`; one-packet activities therefore still return `0x03` immediately.
- Alarm multi-packet handling remains unchanged and hardware-validated.

# BUILD 134 - v0.5.0-dev: large Schedule continuation ACK test + on-screen build ID

- Added a temporary on-screen release/build overlay when the official app changes screen power from OFF to ON. The overlay shows `R0.5` and `B134` for about 1.8 seconds, is rendered directly at the physical-output stage, and does not modify the logical framebuffer or active display mode.
- Refined large Program/Schedule transfer handling after BUILD 133 hardware logs showed that status `0x03` caused the app to advance to the next activity after only the first ~4096 media bytes. In BUILD 134, accepted incomplete `05/80` activity chunks return `0x01` (continue), while the CRC-validated final chunk returns the historically required terminal status `0x03`. Small one-packet activities therefore retain the established `0x03` behavior.
- Added compact `SCH ACK` diagnostics so the next hardware run can confirm whether the official app continues the same large activity after status `0x01`.
- Kept BUILD 132 Alarm multi-packet handling unchanged because it is now hardware-validated.
- Kept the dynamic MatrixPortal `/dev/serial/by-id/usb-Adafruit_MatrixPortal_ESP32-S3*-if00` updater discovery and root-level updater script unchanged.

# BUILD 133 - v0.5.0-dev: Program/Schedule multi-packet media staging

- Extended the BUILD 132 large-media staging model from Alarm to Program/Schedule activity media.
- Schedule `05/80` packets now treat the 23-byte activity header as per-logical-packet framing while `mediaSize` and `mediaCRC` describe the complete media object.
- Media chunks are appended to the per-activity LittleFS staging file and CRC-validated only after the declared complete byte count has been received.
- Preserved the experimentally confirmed Schedule ACK rule: every accepted `05/80` logical packet receives status `0x03`; validation/storage failures use the existing emulator status `0x02`.
- Added a 10-second incomplete-media timeout. A failed or abandoned activity invalidates only the new list-level staging transaction; the currently active Schedule remains untouched.
- The exact 64x64 multi-packet Schedule wire behavior is not yet captured from the app, so the repeated-header chunk model is an implementation hypothesis derived from the confirmed BUILD 132 Alarm behavior and remains subject to hardware validation.
- Release remains `v0.5.0-dev`; internal build advances to 133.

# BUILD 132 - v0.5.0-dev: multi-packet Alarm media and dynamic MatrixPortal serial discovery

- Added 64x64 Alarm media reassembly across multiple complete FA02 logical packets. Hardware captures show that every Alarm chunk repeats the 24-byte Alarm header while `mediaSize` and `mediaCRC` describe the complete media object.
- The first observed chunk uses `reserved2=0x00` and a following chunk uses `reserved2=0x02`; BUILD 132 records that byte diagnostically but determines completion from accumulated byte count and the full-object CRC instead of assuming undocumented flag semantics.
- Alarm media is staged incrementally in `/alarmN.tmp`, protected by a 10-second transaction timeout, checked for metadata consistency/overflow, CRC-verified only after the declared total size is received, then atomically committed with the existing backup/rollback path.
- Repeated first chunks restart the staging transaction rather than duplicating data; unexpected continuation chunks without a known first chunk are rejected internally while preserving the existing compatibility ACK.
- Updated `update_idotmatrix_emulator.sh` to discover MatrixPortal S3 TinyUSB serial devices with `/dev/serial/by-id/usb-Adafruit_MatrixPortal_ESP32-S3*-if00`, allowing the persistent by-id name to include the board MAC address. `SERIAL_PORT` remains available as an explicit override.
- The post-upload detach/reattach loop now checks the dynamic serial pattern, then resolves the new by-id path before opening the monitor.
- No intentional changes to the BUILD 131 HUB75 backend, BLE MTU/framing, Carousel, Text, Snowflake, Clock, Countdown or Stopwatch paths.

# BUILD 131 - v0.5.0-dev: updater cache guard and Alarm capture validation

- Fixed a development-workflow hazard where the updater preserved `.pio/` while `rsync -a` restored archive timestamps, allowing PlatformIO to reuse the previous `IDotMatrix.ino` object even after a source update.
- The updater now removes only the cached `IDotMatrix.ino` object/dependency file and touches the synchronized sketch before the build. Framework and library caches remain intact.
- Added a persistent runtime firmware signature (`IDOTMATRIX_FW=<release>-B<build>`) at boot.
- The updater verifies the same signature in `firmware.bin` after build/upload and aborts the remaining workflow if it does not match the synchronized source.
- Kept `DEBUG_SERIAL_VERBOSE=0` as the default. Packet-level FA02/FA03/RXASM and periodic heap diagnostics remain hidden unless verbose logging is explicitly enabled.
- Alarm parse failures remain visible in normal diagnostics and include parsed sizes plus the raw 24-byte Alarm header. No speculative Alarm format change is made until a BUILD 131 capture is available.
- No display backend, BLE framing, media decoder, Snowflake, Carousel, Clock, Text, Countdown, or Stopwatch runtime behavior is intentionally changed from BUILD 130.

# BUILD 130 - v0.5.0-dev: WLED-native HUB75 backend test and lighter diagnostics

- Replaced the MatrixPortal S3 Adafruit Protomatter output path with the same `ESP32-HUB75-MatrixPanel-DMA` driver family used by WLED.
- Pinned the HUB75 driver to the same 3.0.14 commit currently used by upstream WLED and mirrored the MatrixPortal S3 pinout.
- Configured 8-bit per-channel HUB75 color depth, single DMA buffering, `clkphase=false`, `NO_CIE1931` and the WLED S3 LCD divider setting.
- HUB75 brightness now uses the driver's output-enable brightness control instead of pre-scaling RGB channels. This preserves the full 8-bit framebuffer values and directly targets the coarse fade/banding observed in BUILD 129.
- Kept the distributed Snowflake algorithm unchanged because BUILD 129 already matches the hardware-validated WLED implementation; the backend swap isolates whether the remaining visual banding is output-path related.
- Added `DEBUG_SERIAL_VERBOSE` (default `0`). Per-fragment FA02 reassembly, raw FA03 TX, AE01 raw dumps and periodic heap reports are hidden by default while high-level protocol errors remain visible.
- Expanded Alarm rejection diagnostics with parsed packet/media sizes and the raw 24-byte Alarm header. BUILD 129 logs prove the BLE assembler receives complete Alarm packets, so the remaining failure is now isolated to Alarm payload/header interpretation.
- Updated `update_idotmatrix_emulator.sh` to require a post-upload USB detach followed by reattach before opening the runtime serial monitor. This accommodates MatrixPortal S3/WSL cases where the board remains attached through the programming USB endpoint after flashing.
- No intentional changes to the validated MTU 517 transfer path, Carousel/image/TEXT decoding, Countdown or Stopwatch.

# BUILD 129 - v0.5.0-dev: HUB75 visual consolidation

- Ported the hardware-validated distributed Snowflake particle layout from the WLED Usermod, removing the repeating snow-band / empty-band pattern.
- Added the WLED-derived procedural Carousel transfer indicator: a falling data packet, matrix icon and whole-Carousel progress bar after a 250 ms visibility threshold.
- Reduced Protomatter HUB75 bit depth from 6 to 5 as a controlled test for the visible PWM/refresh artefact observed on the MatrixPortal S3 panel.
- Removed the unused Wi-Fi/ArduinoOTA runtime path from the standalone emulator; USB/PlatformIO upload remains the development workflow.
- Preserved the BUILD 128 MTU 517 transport fix that restored large TEXT, image and Carousel transfers on ESP32-S3.
- Updated the root update helper so the persistent MatrixPortal serial check is mandatory before the combined PlatformIO build+upload step; missing USB attachment remains a prompted retry loop.

# BUILD 128 - v0.5.0-dev: ESP32-S3 BLE MTU alignment and test workflow

- Configured the standalone BLE server local MTU to 517, matching the working WLED/NimBLE implementation.
- BUILD 127 hardware logs showed a characteristic truncation pattern: a 778-byte TEXT packet stopped after exactly 2 x 253-byte ATT writes (506 bytes), while a 2890-byte media packet stopped after exactly 6 x 253-byte writes (1518 bytes). Those write counts are exactly the counts required if the app schedules the same packets for a 514-byte ATT payload, strongly indicating an MTU mismatch rather than a TEXT decoder or LittleFS failure.
- Added a boot diagnostic confirming `BLE local MTU configured: 517`. Existing `[RXASM]` diagnostics remain enabled so the next hardware run can confirm whether incoming ATT writes grow and complete.
- Kept the BUILD 127 32x64 TEXT decoder unchanged: the current large-font failure occurs before a complete TEXT packet reaches the decoder.
- Kept HUB75 Protomatter at 6-bit depth. Hardware feedback indicates fade effects still look coarse; color-depth/gamma/dithering investigation remains separate from the BLE transfer fix.
- Updated `update_idotmatrix_emulator.sh` in the repository root: it now prompts for persistent MatrixPortal serial detection, performs compile + upload in one PlatformIO command, waits for USB serial re-enumeration, then prompts to open the serial monitor.
- The updater continues to preserve `.git/`, `.pio/` and `serial.log` while synchronizing a new source archive.

# BUILD 127 - v0.5.0-dev: 64x64 integration diagnostics and fixes

- Added the repository update/build/serial helper `update_idotmatrix_emulator.sh` at repository root so future source-package synchronization preserves the workflow helper automatically.
- Ported the WLED Usermod FA02 framing fix: one BLE ATT write can now complete one logical packet and continue with bytes belonging to the next packet instead of silently dropping trailing bytes. Split two-byte packet-length headers are preserved as well.
- Added `[RXASM]` diagnostics for large/Bulk logical-packet reassembly to investigate 64x64 image transfers.
- Added 32x64 TEXT glyph support used by the official app's 64-pixel font size: 256 bitmap bytes per glyph, marker family `0x08`/`0x09`, plus strict exact-record-size inference for equivalent marker variants such as `0x0A`.
- Increased TEXT payload capacity to cover the full 64-glyph 32x64 record set and widened the glyph-byte count to 16 bits.
- Increased HUB75 Protomatter bit depth from 4 to 6 for smoother gradient/fade effects; refresh performance is intentionally part of this hardware test.
- BLE connection no longer forces `screenOn=true` or refreshes an empty framebuffer. Display power/content remains controlled by boot policy and app commands.
- Added boot diagnostics for the actual DATA/SPIFFS partition used by Arduino LittleFS.
- Added PlatformIO `time` serial-monitor filtering alongside the ESP32 exception decoder.

# BUILD 126 - v0.5.0-dev: PlatformIO GCC 14 compile fix

- Fixed a strict C++ template type-deduction error exposed by the PlatformIO GCC 14 toolchain in `resetTextPosition()`.
- Replaced the mixed-type `max(int16_t, int)` expression with an explicit signed `int16_t` centering calculation.
- Audited the remaining `min()` / `max()` call sites for the same class of mixed-type error; no additional blocking instance was found.
- Kept the current BLE2902 code unchanged. Arduino-ESP32 emits deprecation warnings for manually adding CCCD descriptors, but these are warnings only and changing BLE behavior is intentionally deferred until the new HUB75/PlatformIO path is validated.
- No runtime rendering or protocol behavior is intentionally changed.

# BUILD 125 - v0.5.0-dev: PlatformIO / pioarduino development environment

BUILD 125 adds a reproducible PlatformIO build path without intentionally changing the tested BUILD 124 runtime behavior.

- Added `platformio.ini` with `matrixportal_s3_hub75_64` as the default reference environment.
- Pinned the ESP32 framework to pioarduino `55.03.311`, corresponding to Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5.
- Pinned FastLED 3.10.3, AnimatedGIF 2.2.3, Adafruit GFX 1.12.6 and Adafruit Protomatter 1.7.1.
- Added an 8 MB MatrixPortal S3 partition table with a 4 MB factory application region, a reserved TinyUF2 region and a 2.9375 MB `spiffs`-subtype data partition mounted by Arduino LittleFS.
- PlatformIO explicitly selects LittleFS so the emulator no longer depends on the Arduino IDE's FAT-oriented default partition choice.
- Made `DISPLAY_BACKEND`, `IDOTMATRIX_SCREEN_TYPE`, physical width/height and `MATRIX_PIN` source defaults overrideable by build flags while retaining the same Arduino IDE defaults.
- Added `docs/PLATFORMIO.md` with build/upload/monitor instructions and partition rationale.
- Added `.pio/` to `.gitignore`.
- Arduino IDE remains supported; PlatformIO becomes the recommended development workflow for the MatrixPortal S3 target.

No intentional BLE protocol, renderer, GIF, Clock, Carousel, Alarm or Schedule behavior change is included in BUILD 125.

# BUILD 124 - v0.5.0-dev: resolution-aware effect bands and MatrixPortal storage guidance

- Hardware validation of BUILD 123 confirmed the MatrixPortal S3 + HUB75 64x64 backend for solid output, clocks and light effects.
- GIF upload failure was traced to the Arduino IDE MatrixPortal S3 default FAT partition scheme, which does not provide the SPIFFS/LittleFS-compatible data partition expected by the emulator.
- Added an explicit boot hint when LittleFS remains unavailable after the configured mount/recovery attempt.
- Documented that FAT-only partition schemes are unsupported by the current storage backend and that a SPIFFS/LittleFS-compatible data partition must be selected.
- Scaled the geometry of band-based light effects with the logical display resolution: 1x on 16x16, 2x on 32x32 and 4x on 64x64.
- Effects 3 and 4 scale their stripe width; effect 5 scales both the colored band and black gap while preserving their original proportions.
- No BLE/protocol semantics were changed.

# BUILD 123 - v0.5.0-dev: selectable display backend and native HUB75 64x64

First development build of the 0.5 line, branched from the stable v0.4.0 / BUILD 118 runtime.

- Added compile-time display backend IDs: `DISPLAY_BACKEND_WS2812` and `DISPLAY_BACKEND_HUB75`.
- Added the first Adafruit Protomatter HUB75 backend for MatrixPortal S3.
- BUILD 123 defaults to logical iDotMatrix profile `0x04` (64x64) and physical 64x64 HUB75 output.
- Logical and physical resolutions are now independent by design.
- Replaced the old optional preview-only rescale path with an automatic final-output scaler for every 16/32/64 logical/physical combination.
- Upscaling uses nearest-neighbour replication; downscaling uses box averaging, ported from the independently validated WLED Usermod policy.
- Rendering/protocol/media code continues to operate exclusively on the logical framebuffer; only the final display stage knows the physical resolution/backend.
- HUB75 brightness is applied by scaling RGB output values before conversion to RGB565; WS2812 retains FastLED brightness control.
- Carried forward generic public-repository defaults: GPIO4 for the optional WS2812 backend, OLED/status LED/buzzer disabled, and LittleFS format-on-mount-failure enabled.
- Added startup diagnostics for backend, logical/physical dimensions and PSRAM availability where supported.
- No iOS diagnostic experiments from BUILD 119-122 are included in this branch.

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
