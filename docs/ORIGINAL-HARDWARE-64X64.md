# Original iDotMatrix 64×64 Hardware Oracle

This document records observations made directly on a physical original iDotMatrix 64×64 unit. It intentionally separates **original-device behavior** from **emulator policy**.

The physical teardown/PCB documentation is still pending. Photographs and component identification will be added after inspection.

## Identification

- Display class: original iDotMatrix 64×64
- Official-app screen type: `0x04`
- Device Information MCU version shown by the official app: `5.11`
- Persistent RTC: not observed
- Device Assets storage: persistent across power cycles

## Direct behavioral observations

### Boot and connection

- A newly initialized/original device can show cosmetic logo/fade behavior.
- Normal subsequent boot resumes the stored Device Assets carousel when present.
- BLE connection on the original unit shows a short visual connection animation/logo.
- The emulator deliberately does not reproduce these cosmetic animations.

### Timekeeping

- The original unit does not retain valid clock time across a power cycle.
- After official-app time synchronization, the software clock continues while the unit remains powered, including after BLE disconnect.
- Alarm operation therefore continues after BLE disconnect but not after a cold reboot until time is synchronized again.

### Device Assets / Carousel

- The Carousel is persistent across power cycles.
- Program/Schedule can preempt Carousel playback.
- After the event ends, the original device returns to Carousel.
- Deleting animations from the app does not necessarily imply immediate deletion of already stored device-side Carousel content.
- Device reset clears stored Device Assets content.

### Buzzer

- Alarm: repeating groups of three short monotone beeps.
- Program/Schedule: the same repeating pattern for roughly 30 seconds.
- Countdown: silent on completion.

### Display controls

- Power Saving reduces brightness.
- Flip rotates the complete display by 180 degrees.
- Countdown and Stopwatch visible behavior broadly matches the emulator implementation.

### Transient app content

- Cloud animation/GIF playback on the original device stops or returns to background content when leaving the relevant app section.
- Graffiti behaves similarly.
- The emulator intentionally preserves these transient visuals instead.

### TEXT effects

Direct visual comparison currently shows:

- Blink: emulator is broadly similar to the original.
- Breathe: emulator is broadly similar to the original.
- Snowflake: the original constructs the text by dropping line/stroke elements from above, resembling boards building a wall. The emulator intentionally keeps its preferred custom snow effect.
- Laser: the original constructs the text from the right with a beam-like reveal. The emulator intentionally keeps its preferred scan-line laser effect.
- Long text: the original consumes the complete phrase rather than clipping to the first visible glyphs. The emulator now implements resolution-independent paging/continuous vertical tape accordingly.

The official app exposes three text sizes for the 64×64 profile and two for the 16×16 profile. The exact third 64×64 glyph record format is still pending capture; no marker or geometry should be claimed until observed.

## Intentional emulator differences

| Original behavior | Emulator policy |
|---|---|
| Cosmetic boot logo/fade | No cosmetic boot animation |
| BLE connection logo/animation | One low-priority 90 ms connection beep; display content is untouched |
| Cloud/Graffiti returns to background when leaving app section | Current content remains visible/running |
| Countdown completion silent | One three-beep completion notification |
| Program/Schedule repeats buzzer for ~30 s | One three-beep notification |
| Original Snowflake line/stroke construction | Custom snow visual effect |
| Original Laser beam/reveal construction | Custom scan-line laser effect |
| No persistent RTC | Optional RTC support for standalone operation |
| Stored Carousel resumes when no valid clock exists | Boot policy: valid RTC → Clock; else valid Carousel → Carousel; else screen off |

These are deliberate product choices, not protocol-compatibility defects.

## Physical inspection plan

When the unit is opened, capture high-resolution photographs before and after separating any stacked boards.

Recommended photo set:

1. Complete enclosure front/back.
2. Internal overview before disconnecting anything.
3. LED matrix rear side.
4. Controller PCB front and back.
5. Macro photographs of every IC and readable marking.
6. Power connector and power-regulation section.
7. Buzzer and its driver circuitry.
8. Bluetooth/MCU area and antenna.
9. Flash/memory devices.
10. Crystals/oscillators.
11. Connectors, ribbon cables and inter-board wiring.
12. Test pads, programming pads and unpopulated headers.
13. PCB revision/model markings.
14. At least one photograph with a ruler or caliper for scale.

## Physical details pending inspection

- MCU / SoC
- Flash capacity and part number
- External RAM, if any
- LED matrix driver architecture
- Power rails and regulators
- Buzzer part / driver
- Antenna implementation
- Board dimensions
- Panel interconnect
- Test/programming interface
- PCB revision identifiers
- Any unpopulated RTC or peripheral footprints

## Research policy

Direct observations from this physical unit take precedence over inference for the tested model. Observations from public implementations remain valuable corroborating evidence but should be labeled separately. Emulator-specific improvements must remain explicitly documented as intentional differences.
