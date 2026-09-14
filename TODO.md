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
- [ ] Test firmware on a physical 64x64 panel
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


## Original hardware physical documentation

- [ ] Photograph the original 64×64 enclosure, matrix, PCB front/back, connectors and wiring
- [ ] Record all readable IC/PCB markings, test pads, regulators, crystals and memory devices
- [ ] Add measured board/enclosure dimensions and power-path notes where practical
- [ ] Update `docs/ORIGINAL-HARDWARE-64X64.md` with the physical inspection results

## ESP32-S3 / HUB75 next phase


- [ ] Add an ESP32-S3 hardware profile
- [ ] Integrate native 64x64 HUB75 output
- [ ] Validate PSRAM-backed buffers/media paths
- [ ] Re-run official-app 64x64 protocol tests on native 64x64 hardware
- [ ] Capture and decode the third 64x64 TEXT glyph size/marker
- [ ] Revisit RTC integration on the new hardware platform

## iOS compatibility investigation


- [x] Obtain failing iOS and successful Android BUILD 119 traces from the same ESP32 hardware
- [x] Confirm that the failing iOS session establishes BLE but sends no FA02/AE01 application writes before app errors
- [ ] Capture FA03/AE02 CCCD subscription behavior with BUILD 120
- [ ] Capture FA03 notification status during the automatic Device Info push
- [ ] Compare emulator advertising/GATT database against an original iDotMatrix unit with nRF Connect
- [ ] Test BUILD 121 with the captured real 32x32 manufacturer identity and record whether iOS begins normal FA02 traffic
- [ ] Verify the reporter's exact iDotMatrix iOS app version (`1.0.9` versus possible `1.9.0` typo)
- [ ] Determine whether any emulator-side compatibility workaround is justified by evidence

## Buzzer hardware follow-up


- [ ] Test the ordered passive buzzer and decide whether tone/frequency-capable output should be added alongside the existing active-buzzer implementation
