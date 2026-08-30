# TODO / Project status

This file intentionally separates completed features, partial features, optional hardware, known issues and open protocol questions.

## Completed

- [x] BLE advertising recognized by the app.
- [x] FA and AE services and FA02/FA03/AE01/AE02 characteristics.
- [x] Device info / 16x16 matrix identification.
- [x] Date and time synchronization from the app.
- [x] Matrix power on/off.
- [x] 180-degree rotation.
- [x] Brightness 0..100 with configurable hardware limit.
- [x] Brightness persistence in NVS.
- [x] Time-window power saving.
- [x] Solid color.
- [x] Graffiti/DIY mode.
- [x] 16x16 RAW RGB images.
- [x] Animated GIFs.
- [x] Text and glyphs with correct bitmap orientation.
- [x] Text colors and effects investigated.
- [x] Visual effects investigated and visually refined.
- [x] Clock and clock styles investigated.
- [x] Scoreboard.
- [x] Audio: 5 LEVEL modes.
- [x] Audio: 5 FFT modes.
- [x] Alarms: parsing and 10 persistent slots.
- [x] Alarms: duration, weekdays, one-shot behavior and media.
- [x] Programs/Schedules: global ON/OFF state and sound flag.
- [x] Programs/Schedules: correct handshake `07 80 -> 01`, `05 80 -> 03`.
- [x] Programs/Schedules: at least 12 activities verified; firmware limit 32.
- [x] Programs/Schedules: NVS + LittleFS persistence.
- [x] Programs/Schedules: staging and commit of a new activity list.
- [x] Programs/Schedules: automatic execution by weekday and time range.
- [x] Programs/Schedules: TEXT.
- [x] Programs/Schedules: GIF.
- [x] Programs/Schedules: 16x16 PNG.
- [x] PNG decoder with inflater state on heap, avoiding stack overflow.
- [x] Optional U8g2 diagnostic OLED.
- [x] OLED: build, BLE, screen, mode, time, brightness, Schedule and timer status.
- [x] OLED: prominent alert for unhandled commands with raw-byte preview.
- [x] BUILD 62 verified on hardware with pure event-driven OLED and smooth matrix animations.

## Partially completed

### Countdown

- [x] MODE 0/1/2/3 decoded.
- [x] Local start, pause, resume and reset.
- [x] Remaining-time rendering.
- [x] Spontaneous `08 80 03` event at natural completion.
- [ ] Determine why the app UI still does not perfectly reproduce the expected behavior.

### Stopwatch

- [x] MODE 0/1/2/3 decoded.
- [x] Local start, pause, resume and reset.
- [x] Internal timekeeping verified.
- [x] Sniffer confirmed that the app performs no periodic polling.
- [x] ACK `01/03` variants tested without solving the UI issue.
- [ ] Determine the response/state expected from the original device.

### Text

- [x] Global structure and glyph bitmaps.
- [x] Correct bit orientation (`bit0 = left`).
- [ ] Decode the semantic meaning of all 7 glyph `META` bytes.

### ACK semantics

- [x] Standard ACK `01` verified for many commands.
- [x] `03` confirmed as required for bulk completion and Schedule activities.
- [ ] Formalize the general meaning of `01`, `02`, `03` across all sub-protocols.

## Optional hardware

### Buzzer

- [x] Protocol fields and firmware hooks implemented.
- [ ] Select a free GPIO and test a real buzzer.

### DS3231 RTC

- [x] Optional compile-time support prepared.
- [x] BLE time sync can update the RTC when enabled.
- [ ] Test on real hardware if standalone timekeeping after power loss is desired.

The current development board does not contain a battery-backed RTC. Without an external RTC, alarms and Schedules rely on a BLE time sync after a full power cycle.

## Known issues

- Stopwatch app-side behavior is still not fully reproduced.
- Countdown app-side behavior should receive another compatibility pass.
- Some TEXT metadata bytes remain unknown.
- The exact semantics of some reserved Bulk/Alarm/Schedule header fields remain unknown.
- Some visual/audio effects are visual approximations rather than mathematically exact clones.

## Open protocol questions

1. What exact FA03 response does the original device send for stopwatch commands `09 80`?
2. What is the general semantic meaning of ACK status `01`, `02` and `03`?
3. What do all 7 TEXT glyph META bytes represent?
4. What do the remaining reserved Alarm/Schedule/Bulk header bytes mean?
5. Are there app features/commands that have not yet been exercised?
6. Does the original hardware contain any persistent RTC, or does it rely entirely on app time synchronization after power loss?

## Future software improvements

- [ ] Split the monolithic `.ino` into modules once protocol work is sufficiently stable.
- [ ] Add automated parser tests based on the captures in `docs/captures/`.
- [ ] Add a compact protocol regression test harness.
- [ ] Consider hardware I2C for the OLED only if future diagnostic needs require frequent refreshes.
- [ ] Add more annotated captures for TEXT, Alarm content types and unknown commands.

## Rule for new discoveries

When a new command is observed:

1. preserve the raw capture in `docs/captures/`;
2. document confirmed behavior in `PROTOCOL.md`;
3. add unresolved questions here;
4. update `HISTORY.md` when the behavior is implemented or corrected.
