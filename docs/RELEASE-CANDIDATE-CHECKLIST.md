# v0.4.0 Release Candidate Hardware Checklist

This checklist is intended for the final hardware regression pass of `v0.4.0-dev / BUILD 117` before promoting the project to the public `v0.4.0` release.

## Build and startup

- [ ] Arduino project compiles cleanly with the validated ESP32 core/libraries.
- [ ] Device boots without unexpected reset loops.
- [ ] OLED startup banner reports the current build number.
- [ ] BLE advertising is visible to the official iDotMatrix app.
- [ ] Official app connects successfully.
- [ ] One short connection beep is emitted when the buzzer is idle.
- [ ] Connection beep does not interrupt an active Alarm or other buzzer notification.
- [ ] Existing matrix content is not replaced by a connection animation.

## Device information and basic controls

- [ ] Device Information shows the expected app-facing MCU version for release `0.4.x` (`00 04` → `0.04` in the current official app).
- [ ] Screen ON/OFF works.
- [ ] Brightness works.
- [ ] Power Saving/ECO changes output brightness as expected.
- [ ] 180-degree Flip works.

## Clock and time

- [ ] App time synchronization updates the software clock.
- [ ] Clock effects render correctly.
- [ ] Time colon blinks once per second.
- [ ] Date display uses `/` and remains stable.
- [ ] No clock-layout regression is visible in effects 0-7.

## TEXT

Use at least one string longer than the visible capacity, for example `Piergiorgio`.

- [ ] PIN consumes the complete phrase instead of clipping the first visible glyphs.
- [ ] UP scroll consumes the complete phrase.
- [ ] DOWN scroll consumes the complete phrase.
- [ ] UP/DOWN transitions are continuous with only the intended 1 px page gap and no blank-screen interval.
- [ ] LEFT/RIGHT still scroll the complete line continuously.
- [ ] Blink pages through the complete phrase.
- [ ] Breathe pages through the complete phrase.
- [ ] Snowflake pages through the complete phrase without resetting the snow phase.
- [ ] Laser pages through the complete phrase without resetting the laser phase.
- [ ] Solid color and Rainbow scroll at the same spatial speed for the same app speed setting.
- [ ] Dynamic color/effect redraw remains visually smooth.
- [ ] SimSun/SimHei changes still arrive as app-rasterized glyph bitmaps.

## GIF / Cloud / Graffiti

- [ ] Cloud GIF upload succeeds.
- [ ] GIF continues playing after leaving the app section (intentional emulator behavior).
- [ ] Graffiti remains displayed after leaving the app section (intentional emulator behavior).
- [ ] Large GIF playback does not cause heap corruption or BLE disconnect.

## Device Assets / Carousel

- [ ] Device Assets replacement blacks the physical matrix during upload.
- [ ] GIF slots are stored and played.
- [ ] TEXT slots are stored and played.
- [ ] Mixed GIF/TEXT bank rotates through all populated slots.
- [ ] Per-slot dwell works.
- [ ] Carousel continues after BLE/app disconnect.
- [ ] Stored Carousel resumes according to the boot policy when no valid RTC time takes priority.

## Alarm / Schedule / timers

- [ ] Alarm triggers at the expected time.
- [ ] Alarm uses the repeating three-beep pattern.
- [ ] Schedule/Program starts the expected content.
- [ ] Schedule/Program uses one three-beep notification only.
- [ ] Alarm/Schedule preemption and restore behavior is correct.
- [ ] Countdown completes with one three-beep notification.
- [ ] Stopwatch remains functional.
- [ ] Countdown/Stopwatch rendering has no regression.

## Reset and persistence

- [ ] App-issued reset leaves BLE connected.
- [ ] App-issued reset preserves the current volatile synchronized time.
- [ ] App-issued reset leaves the logical display ON with a black framebuffer.
- [ ] The next app command becomes visible immediately after reset.
- [ ] Persistent Carousel/Alarm/Schedule/brightness/ECO/rotation state is cleared according to emulator policy.
- [ ] Password remains documentation-only; no experimental password runtime is present.

## Diagnostic hygiene

- [ ] Normal Serial output is usable without TEXT/Bulk packet flooding.
- [ ] `TEXT_PROTOCOL_DEBUG` can still be enabled manually when needed.
- [ ] `BULK_PROTOCOL_DEBUG` can still be enabled manually when needed.
- [ ] Unknown-command OLED diagnostics still work.

## Promotion criterion

If this checklist passes without new regressions, BUILD 117 can be used as the runtime baseline for the final `v0.4.0` packaging pass. The final packaging step should change the public release identifier from `0.4.0-dev` to `0.4.0`, assign the final internal build number, and perform documentation-only release cleanup unless a regression is found.

## Final release result


BUILD 117 was used as the release-candidate runtime baseline. The final public package is `v0.4.0 / BUILD 118`. BUILD 118 changes release metadata and documentation only; no runtime feature changes were intentionally introduced after the RC.
