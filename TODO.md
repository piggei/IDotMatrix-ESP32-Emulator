# BUILD 142 iOS experiment / remaining validation

- [x] Capture `TEXT PJ -> image -> TEXT Ciao`: Bulk objects use slots 14, 15 and 16 and activate with `06/02 03 0E 0F 10`.
- [x] Confirm mixed Preset media: TEXT (`type=3`) and GIF/image (`type=1`) coexist in one list.
- [x] Capture a five-image Preset using slots 14..18, including 48,456-byte and 66,361-byte multi-packet objects.
- [x] Confirm all captured Preset objects use `timeSign=5`, including TEXT and GIF/image; it is therefore not treated as the observed ~3 s dwell value.
- [x] Confirm `06/02` carries only count plus ordered slot IDs and no explicit dwell.
- [x] Hardware-test BUILD 140 with `TEXT PJ -> image -> TEXT Ciao`: static TEXT and GIF use ~3 s while scrolling TEXT waits for the final glyph to leave the display.
- [x] Hardware-test a five-image Preset and confirm slots 14..18 rotate in command order.
- [x] Confirm selecting another Preset replaces the active volatile bank cleanly, including replacement while the previous Preset is already playing and while large multi-packet assets are uploaded.
- [ ] Confirm Preset playback survives BLE disconnect while power remains applied.
- [ ] Confirm Preset media is not restored after emulator reboot/reset.

- [x] Disable verbose Preset protocol diagnostics by default for the consolidation baseline; keep the compile-time switch available.
- [x] Align README/PROTOCOL/HISTORY/capture documentation with hardware-validated Preset timing and BUILD 137 Schedule findings.

## BUILD 137 Schedule validation

- [x] Decode the BUILD 135 `1 -> 513` type change: offset 10 is the actual one-byte media type, offset 11 is a separate chunk marker (`00` first, `02` continuation in current captures).
- [x] Save one Program containing exactly one multi-packet image and confirm continuation accumulation without `SCH OBJECT CHANGE` (hardware-validated in BUILD 137).
- [x] Confirm `SCH HDR` reports stable media type `t=1` while `m=0x0` changes to `m=0x2` on continuation packets (hardware-validated in BUILD 137).
- [x] Confirm the final media chunk reaches the declared byte count, `done=1`, receives `SCH ACK ... status=3`, and is followed by `SCH COMMIT` (hardware-validated in BUILD 137).
- [x] Verify timed Program playback after commit using a currently active time window; `SCH START i=...` observed in BUILD 137.
- [x] Repeat with a multi-activity Program; multiple activities committed and scheduled in BUILD 137.
- [ ] Hardware-validate the updater sequence: compile only -> firmware signature check -> second serial/usbip check -> upload -> post-upload runtime reattach.

## BUILD 134 hardware findings

- [x] Screen OFF -> ON from the official app shows the temporary `R0.5` / `B134` overlay without changing the logical display mode (`VERSION SPLASH R0.5 B134` observed).
- [x] Intermediate Schedule ACK `0x01` makes the 64x64 app continue transmitting a large activity.
- [x] The app transmitted exactly the declared 48,456 media bytes in the captured large-image test.
- [x] Identified first-chunk loss in emulator staging: totals `4096 -> 4096 ... -> 44360/48456` prove the first 4096 bytes were discarded when chunk 2 caused an activity-header metadata mismatch/reset.
- [x] Large Program commit/execution validated on BUILD 137, including multi-activity scheduling.

## v0.5.0-dev BUILD 132 validation

- [x] Hardware-validate multi-packet 64x64 Alarm media storage and playback (observed capture: 4096 + 2610 bytes for one 6706-byte GIF).
- [ ] Confirm Alarm replacement remains atomic if a multi-packet transfer is interrupted before completion.
- [ ] Capture a 3+ packet Alarm media transfer, if the official app produces one, to characterize `reserved2` beyond the observed `0x00` then `0x02` sequence.
- [ ] Verify dynamic MatrixPortal `/dev/serial/by-id/usb-Adafruit_MatrixPortal_ESP32-S3*-if00` discovery across upload detach/reattach.

## v0.5.0-dev BUILD 133/134 historical Schedule findings

- [x] Large 64x64 Program/Schedule activity spans many complete `05/80` logical packets with a repeated 23-byte framing header.
- [x] `ACK 03` after an incomplete large chunk is terminal from the app's point of view; it advances/stops rather than continuing the same object.
- [x] `ACK 01` after an incomplete large chunk requests continuation; `ACK 03` remains required when the activity is complete.
- [ ] Interrupt a large Program transfer mid-activity and confirm that the previous active Schedule remains intact.
- [x] Characterize the changing Schedule header byte: offset 11 changes `0x00 -> 0x02` between first and continuation chunks; exact semantic label remains open.

# TODO

## Completed / verified

- [x] Stable 16x16 hardware profile
- [x] BLE app discovery and core protocol handling
- [x] 32x32 (`0x03`) app-profile emulation
- [x] 64x64 (`0x04`) app-profile emulation
- [x] 64x64 cloud GIF reception and playback on classic ESP32
- [x] Large GIF streaming to LittleFS
- [x] Safe GIF RX/PLAY isolation and decoder lifecycle
- [x] 8x16 and 16x32 text glyph parsing
- [x] SimSun/SimHei app-side rasterization identified
- [x] Active buzzer on GPIO18 with non-blocking trill (hardware-verified for Alarm/Schedule)
- [ ] Hardware-validate BUILD 98+ one-shot three-pulse buzzer at natural Countdown completion
- [x] Stopwatch/countdown colour behaviour
- [x] Event-driven OLED diagnostics
- [x] App-facing Device Info version encoding validated: `00 04` is displayed as MCU `0.04` by the official app (BUILD 109)
- [x] Long TEXT paging for PIN and all non-horizontal effects, including continuous UP/DOWN tape (BUILD 111-115)
- [x] Decouple dynamic color/effect redraw from TEXT motion speed (BUILD 116)
- [x] Low-priority single BLE connection beep as intentional replacement for the original connection animation (BUILD 113)

## Hardware validation

- [ ] Test firmware on a physical 32x32 iDotMatrix-compatible panel
- [x] Test firmware on a physical 64x64 HUB75 panel using MatrixPortal ESP32-S3
- [ ] Capture/sniff traffic from an original 32x32 device when available
- [x] Evaluate ESP32-C3 as an alternate target: boots with adapted pin mapping, but current FastLED channel/driver timeouts make it unsupported for v0.4.0

## RTC / standalone operation

- [ ] Integrate optional RTC module
- [ ] Validate RTC state/power-loss detection
- [ ] Start directly in clock mode at power-on when a valid RTC is present
- [ ] Update RTC whenever the app sends a valid time sync
- [ ] Verify Alarm/Schedule operation after a cold boot without the phone

## Protocol / UI

- [x] Correct Device Assets capacity to one 12-slot bank; the app's three 12-position pages are app-side sets, not 36 device slots
- [x] Confirm `imageIndex=0,1,2` for three-item pages in official-app captures
- [x] Confirm `timeSign=5` and `timeSign=30` in official-app captures
- [x] Observe that `0A/01` may precede a later page push and is not necessarily repeated after the Bulk uploads
- [x] Identify BUILD 94 failure: a TEXT Bulk inside a 12-position push cleared carousel upload state, causing GIF indices 6..11 to fall back to live playback
- [x] Hardware-validate BUILD 95 with the same 3-item and 12-position pages
- [ ] Confirm from BUILD 95 diagnostics that the interleaved TEXT Bulk carries `imageIndex=5` and the expected `timeSign`
- [x] Verify mixed GIF/TEXT carousel playback over multiple complete cycles
- [ ] Confirm that a 5-minute selection produces `timeSign=300`
- [x] Verify locally that a running carousel continues after phone Bluetooth/app disconnect
- [x] Consolidate Device Assets implementation/documentation after successful BUILD 96 hardware validation (BUILD 97)
- [x] Hardware-validate BUILD 96 matrix blackout during Device Assets upload
- [ ] Test carousel behavior across ESP32 power cycle separately
- [ ] Capture a page containing other non-GIF media types, if the app permits them, before adding support beyond observed TEXT
- [ ] Continue documenting unknown commands found on larger profiles
- [ ] Compare behaviour against original hardware captures
- [x] Keep WLED integration as a separate project (`IDotMatrix-WLED-UserMod`) rather than coupling it to this standalone emulator

## Reference implementation technical debt

- [ ] Document/reproduce the exact Arduino-ESP32 and library versions used for validated builds
- [x] Validate incoming time/date fields before they can reach calendar logic (BUILD 82)
- [x] Add reassembly and bulk-transfer timeouts plus cleanup of abandoned RX files (BUILD 82; false multi-packet timeout after BUILD 86 mutex integration fixed in BUILD 88)
- [x] Make Alarm/Schedule state and filesystem replacement transactional where practical (BUILD 83: staging, backup, validation and best-effort rollback/recovery)
- [x] Resolve Alarm/Schedule preemption and restore semantics explicitly (BUILD 83: Alarm preempts Schedule first; framebuffer-backed restore is exact, dynamic modes fall back to clock/blank)
- [x] Make ECO brightness transitions independent of renderer refresh and validate incoming ECO fields (BUILD 85)
- [x] Make runtime soft reset clear stale transient/Alarm/Schedule/GIF state deterministically (BUILD 85)
- [x] Gate optional RTC consumers when `lostPower()` indicates invalid time (BUILD 85; physical RTC validation still pending)
- [x] Add compile-time safeguards before OTA can be enabled with placeholder/default credentials (BUILD 87)
- [x] Disable implicit LittleFS formatting and add an explicit opt-in recovery/first-use format path (BUILD 87)
- [x] Move Alarm/Schedule GIF playback from the legacy full-RAM path to an isolated LittleFS PLAY architecture (BUILD 89; hardware-verified)
- [x] Serialize shared BLE/runtime state across the BLE task and Arduino loop with a FreeRTOS task mutex (BUILD 86; timestamp ordering hotfix in BUILD 88)
- [x] Remove the blocking disconnect delay from the BLE callback; advertising restart is deferred to `loop()` (BUILD 86)
- [ ] Remove or defer any remaining callback-heavy operations from latency-sensitive BLE paths
- [ ] Move filesystem/bulk processing out of BLE callbacks where practical
- [ ] Verify the experimentally unconfirmed TEXT compatibility aliases `0x03` and `0x06` against original hardware/app captures
- [ ] Refine bulk ACK error semantics (`0x02`/`0x03`) using original-hardware captures
- [x] Keep transport, filesystem, decoder and RAM media limits documented separately (BUILD 87 documentation pass)

## Original 64×64 protocol-oracle follow-up

Verified on original hardware and no longer open: persistent Carousel boot resume; no persistent RTC; Alarm continues after BLE disconnect once time is synchronized; Alarm repeating triple-beep; Program repeating triple-beep on the original; silent original Countdown; Power Saving brightness reduction; 180-degree Flip; Countdown/Stopwatch visible behavior; reset clears Device Assets.

Still open:

- [x] capture and decode the observed six-digit password SET framing (`04/02`) and decimal-pair encoding;
- [x] observe app-side per-device password caching (`pwdByMac`) and 7-byte write / 5-byte notification timing during password attempts;
- [ ] determine the complete SET-password completion transaction expected by the official app; BUILD 101-103 experiments were removed in BUILD 104 because the app remained on the SET screen;
- [ ] determine whether original-device authentication enforcement is device-side, app-side, or shared before reintroducing password support;
- [x] validate the app-facing MCU version path: advertising changes alone do not control the displayed value; the 9-byte Device Info response does (BUILD 108-109). `FW_BUILD` remains intentionally internal;
- [ ] precisely characterize original Program buzzer duration if useful for documentation (the emulator intentionally uses one trill);
- [ ] complete dedicated Text/Effect protocol captures on the original 64×64, especially the third text size and any 64x64-specific glyph marker/geometry;
- [ ] verify which additional original-device settings survive/reset across `03/80` if protocol fidelity becomes important.


## iOS compatibility branch

- [x] Preserve BUILD 122 baseline evidence: iOS connects, subscribes to FA03 and AE02, keeps the link open, but sends no FA02/AE01 application traffic when unsolicited Device Info is suppressed.
- [x] Confirm the same emulator accepts a manual FA02 Clock command through LightBlue, excluding the basic write path / Clock parser as the immediate blocker.
- [x] Add a current-code classic-ESP32 PlatformIO target for Thiago: logical 32x32, physical 16x16 WS2812, GPIO17.
- [ ] Hardware-test BUILD 142 delayed Device Info after both CCCDs are enabled.
- [ ] Record whether iOS identifies the emulator as 32x32 after the delayed Device Info notification.
- [ ] Record whether any FA02 or AE01 application traffic begins after the delayed Device Info notification.
- [ ] If BUILD 142 remains silent, capture GATT/service discovery against an original device before introducing speculative AE/RCSP replies.

## Original hardware physical documentation

- [ ] Photograph the original 64×64 enclosure, matrix, PCB front/back, connectors and wiring
- [ ] Record all readable IC/PCB markings, test pads, regulators, crystals and memory devices
- [ ] Add measured board/enclosure dimensions and power-path notes where practical
- [ ] Update `docs/ORIGINAL-HARDWARE-64X64.md` with the physical inspection results

## ESP32-S3 / HUB75 next phase


- [x] Add an ESP32-S3 hardware profile
- [x] Integrate native 64x64 HUB75 output
- [ ] Validate PSRAM-backed buffers/media paths
- [x] Re-run official-app 64x64 protocol tests on native 64x64 hardware
- [ ] Capture and decode the third 64x64 TEXT glyph size/marker
- [ ] Revisit RTC integration on the new hardware platform

## v0.5 native display backend validation

- [ ] Compile BUILD 123 against Arduino-ESP32 3.3.x + Adafruit Protomatter 1.7.1 + Adafruit GFX 1.12.6
- [x] Validate native 64x64 logical -> 64x64 HUB75 output on MatrixPortal S3
- [ ] Validate 32x32 logical -> 64x64 physical upscale
- [ ] Validate 16x16 logical -> 64x64 physical upscale
- [ ] Validate 16x16 logical -> 32x32 physical upscale
- [ ] Validate 64x64 logical -> 32x32 and 16x16 physical downscale
- [ ] Validate 32x32 logical -> 16x16 physical downscale
- [x] Re-run major feature coverage (Clock, Text, GIF, Carousel, Graffiti, timers, brightness, flip) on HUB75
- [ ] Decide which large media/frame buffers should move to PSRAM after baseline validation

## BUILD 124 follow-up

- [x] Scale band-based light-effect geometry with logical resolution (16x16=1x, 32x32=2x, 64x64=4x)
- [x] Document MatrixPortal S3 Arduino partition requirement for LittleFS-backed media
- [x] Hardware-test GIF upload/playback on MatrixPortal S3 after selecting a SPIFFS/LittleFS-compatible partition scheme
- [ ] Hardware-compare 32x32 and 16x16 logical profiles on the 64x64 physical HUB75 panel using automatic scaling

## PlatformIO migration

- [x] Add MatrixPortal S3 / HUB75 64x64 PlatformIO environment
- [x] Pin Arduino-ESP32 3.3.11 reference toolchain through pioarduino
- [x] Encode a LittleFS-compatible 8 MB partition layout in the repository
- [ ] Hardware-build and upload BUILD 125 with PlatformIO on Windows
- [ ] Compare first clean-build and incremental-build times against Arduino IDE
- [ ] Add validated PlatformIO environments for legacy WS2812 and 16x16/32x32 logical/physical combinations as those configurations are re-tested

## BUILD 127/128 hardware validation

- [x] Confirm LittleFS partition discovery/mount on MatrixPortal S3 from a complete boot log.
- [x] Capture BUILD 127 large-transfer `[RXASM]` diagnostics. The app stopped a 778-byte TEXT packet after 506 bytes (2 x 253) and a 2890-byte media packet after 1518 bytes (6 x 253).
- [x] Validate local MTU 517: large TEXT/GIF/Carousel/Alarm/Schedule/Preset transfers now reach the protocol handlers completely on MatrixPortal S3.
- [x] Retest Cloud/static/GIF image transfers after the MTU change.
- [x] Retest 64-pixel TEXT / 32x64 glyph rendering after the MTU change.
- [x] Compare fade effects at HUB75 bit depth 6 against BUILD 126 bit depth 4: hardware feedback still reports visibly poor/coarse fades.
- [ ] Investigate HUB75 gradient quality separately (effective PWM depth, gamma and/or dithering) without mixing that experiment into the BLE MTU fix.
- [ ] Confirm BLE connect no longer changes an OFF/black boot state.

- [x] Hardware-test BUILD 129: Carousel/image/TEXT transfers remain functional; Snowflake visual banding and coarse HUB75 fades remain; Alarm full-media packets are received completely but rejected by header/media validation.
- [ ] Hardware-validate BUILD 130 WLED-native HUB75 DMA output for fade quality, flicker/dithering and Snowflake appearance.
- [ ] Capture BUILD 130 Alarm header diagnostics and compare the live 64x64 Alarm packet layout with the documented 24-byte header.

## iOS / RCSP

- [x] Capture BUILD 143 connection logs from the official iOS app: iOS subscribed to FA03/AE02 but produced no FA02, AE01, RCSP or raw-auth traffic.
- [ ] Capture BUILD 144 connection logs and determine whether the unsolicited AE02 raw-auth stimulus triggers any AE01 response.
- Capture the real iDotMatrix `GET_TARGET_FEATURE` (`0x03`) response before implementing MCU-version emulation.
- If AE01 remains silent, obtain a short iPhone <-> original iDotMatrix connection capture focused on AE01/AE02.
