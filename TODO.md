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
- [x] Active buzzer on GPIO18 with non-blocking trill
- [x] Stopwatch/countdown colour behaviour
- [x] Event-driven OLED diagnostics

## Hardware validation

- [ ] Test firmware on a physical 32x32 iDotMatrix-compatible panel
- [ ] Test firmware on a physical 64x64 panel
- [ ] Capture/sniff traffic from an original 32x32 device when available
- [ ] Validate ESP32-C3 SuperMini as a compact target

## RTC / standalone operation

- [ ] Integrate optional RTC module
- [ ] Validate RTC state/power-loss detection
- [ ] Start directly in clock mode at power-on when a valid RTC is present
- [ ] Update RTC whenever the app sends a valid time sync
- [ ] Verify Alarm/Schedule operation after a cold boot without the phone

## Protocol / UI

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
- [x] Move Alarm/Schedule GIF playback from the legacy full-RAM path to an isolated LittleFS PLAY architecture (BUILD 89)
- [x] Serialize shared BLE/runtime state across the BLE task and Arduino loop with a FreeRTOS task mutex (BUILD 86; timestamp ordering hotfix in BUILD 88)
- [x] Remove the blocking disconnect delay from the BLE callback; advertising restart is deferred to `loop()` (BUILD 86)
- [ ] Remove or defer any remaining callback-heavy operations from latency-sensitive BLE paths
- [ ] Move filesystem/bulk processing out of BLE callbacks where practical
- [ ] Verify the experimentally unconfirmed TEXT compatibility aliases `0x03` and `0x06` against original hardware/app captures
- [ ] Refine bulk ACK error semantics (`0x02`/`0x03`) using original-hardware captures
- [x] Keep transport, filesystem, decoder and RAM media limits documented separately (BUILD 87 documentation pass)
