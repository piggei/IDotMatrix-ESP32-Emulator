# HISTORY

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
- Correct handling of multiple activities and return to the previous display mode.

### Protocol
- Discovered that Schedule activity ACK must end with status `0x03`; `0x01` makes the app stop sending and report an error.
- Implemented sequential reception of programs with many activities.

## Alarm / Schedule development period

### Added
- 10 persistent Alarm slots in flash/LittleFS.
- Alarm-associated media: GIF, images and text according to received payloads.
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

## Build 67

### Added
- Active buzzer support on GPIO 18 with a non-blocking three-pulse trill pattern for alarms and schedules.
- Resolution-aware text glyph handling for 8x16 (`0x02`) and 16x32 (`0x05`) glyph records.

### Changed
- 16x16 restored as the default physical/logical profile while retaining the multi-size framework introduced for 32x32 experiments.
- Stopwatch rendering is white.
- Countdown rendering is white, switching to red for the final five seconds.
- Schedule-specific verbose debug mode removed; normal protocol diagnostics remain available.

### Protocol / research
- The official app was successfully exercised with the emulator advertising the 32x32 profile (`screen type 0x03`).
- The app exposes 16/32 text size selection for the 32x32 profile.
- SimSun and SimHei selection changes glyph bitmap data rather than sending a device-side font-selection command.
- 32x32 cloud GIF transfers were observed using 4096-byte chunks, with `0x01` intermediate acknowledgements and `0x03` completion acknowledgement.
- Cloud assets differ by screen profile, while clock style identifiers appear unchanged and are scaled by the device renderer.
- Graffiti coordinates operate correctly with the simulated 32x32 profile.
- Programs created under the 16x16 profile were not shown after switching to 32x32, suggesting app-side per-profile storage.

## Builds 63-66

- B63 introduced the first multi-size / simulated 32x32 device profile and removed several hard-coded 16x16 assumptions.
- B64 added focused TEXT and bulk-transfer diagnostics used to identify 16px and 32px glyph formats.
- B65 consolidated multi-size text handling and added the first active-buzzer integration.
- B66 introduced the non-blocking buzzer trill; B67 fixes declaration ordering and is the consolidated build.
