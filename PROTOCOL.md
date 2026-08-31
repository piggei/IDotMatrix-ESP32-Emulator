# iDotMatrix BLE Protocol - reverse-engineering notes

> Cross-project validation and model comparison: see [`docs/PROTOCOL-COMPARISON.md`](docs/PROTOCOL-COMPARISON.md).


This document describes the protocol observed between the iDotMatrix app and the ESP32 emulator.

Because no original device was available, the information comes from app traffic captures and differential tests: one app setting is changed at a time and the resulting packets are compared.

## Conventions

Confidence levels:

- **CONFIRMED** - behavior verified repeatedly;
- **PARTIAL** - working command, but some fields or semantics are not fully identified;
- **UNKNOWN** - packet observed but not decoded.

All hexadecimal values are written as separate bytes. Multi-byte fields observed in the protocol are generally **little-endian**.

## BLE transport

### Services and characteristics - CONFIRMED

| Element | UUID | Observed use |
|---|---|---|
| FA service | `000000fa-0000-1000-8000-00805f9b34fb` | main channel |
| FA02 | `0000fa02-0000-1000-8000-00805f9b34fb` | App -> device, write |
| FA03 | `0000fa03-0000-1000-8000-00805f9b34fb` | Device -> app, notify/read |
| AE service | `0000ae00-0000-1000-8000-00805f9b34fb` | secondary channel |
| AE01 | `0000ae01-0000-1000-8000-00805f9b34fb` | App -> device |
| AE02 | `0000ae02-0000-1000-8000-00805f9b34fb` | Device -> app |

The emulator advertising exposes the FA service and the observed manufacturer data:

```text
54 52 00 70 01
```

### Packet length

In FA02 packets the first `uint16` normally contains the total packet length, little-endian. Example:

```text
05 00 09 80 01
```

`05 00` = 5 total bytes.

The BLE callback may receive one logical packet across multiple writes; the firmware accumulates data until the declared length is reached.

## Responses / ACK

### Standard ACK - CONFIRMED

The most common form is:

```text
05 00 CMD SUB STATUS
```

Example:

```text
05 00 04 80 01
```

`STATUS=01` is used as the normal acknowledgement/acceptance status for many commands.

### Status 03 - CONFIRMED, general semantics PARTIAL

`03` is used in at least two important cases:

1. completion of a bulk transfer;
2. ACK for one complete Schedule activity.

For Schedules the distinction is critical:

```text
07 80 -> ACK 01
05 80 -> ACK 03
```

Replying with `01` to a `05 80` activity makes the app report an error and prevents subsequent activities from being sent. With `03`, the app continues correctly.

The universal semantics of `01/02/03` are not yet considered fully decoded.

---

# General commands

## Device info - CONFIRMED

### Query

```text
04 00 01 80
```

### Observed/emulated response

```text
09 00 01 80 04 0E 01 01 00
```

This response is sufficient for the app to recognize the device as a 16x16 matrix.

## Date/time synchronization - CONFIRMED

11-byte packet:

```text
0B 00 01 80 YY MM DD ? HH MI SS
```

Fields used by the firmware:

| Offset | Field |
|---:|---|
| 4 | year as `2000 + YY` |
| 5 | month |
| 6 | day |
| 7 | field not yet used/identified with certainty |
| 8 | hour |
| 9 | minute |
| 10 | second |

Captured example:

```text
0B 00 01 80 1A 08 1E 07 14 0B 35
```

The firmware uses this synchronization as the base for its software clock. If `RTC_ENABLED=1`, the same command can also synchronize a DS3231.

ACK:

```text
05 00 01 80 01
```

## Matrix power on/off - CONFIRMED

```text
05 00 07 01 STATE
```

`STATE`:

- `00` = off;
- any non-zero value = on.

Standard ACK `01`.

## 180-degree rotation - CONFIRMED

```text
05 00 06 80 STATE
```

`00` disables rotation; any non-zero value enables it.

## Brightness - CONFIRMED

```text
05 00 04 80 PERCENT
```

`PERCENT` is 0..100. The firmware scales it to `MAX_LED_BRIGHTNESS` and stores it in NVS using delayed writes.

## Power saving - PARTIAL

```text
0A 00 02 80 ENABLE SH SM EH EM REDUCTION
```

Implemented interpretation:

| Field | Meaning |
|---|---|
| ENABLE | enable flag |
| SH:SM | start time |
| EH:EM | end time |
| REDUCTION | reduction percentage |

The logic also supports time ranges crossing midnight.

## Runtime soft reset - PARTIAL

```text
04 00 03 80
```

The firmware replies with an ACK and resets runtime graphics state shortly afterward. Exact correspondence with original hardware behavior cannot be verified.

---

# Graphic content

## Solid color - CONFIRMED

```text
07 00 02 02 R G B
```

The three channels are 8-bit RGB.

## Graffiti / DIY mode - CONFIRMED

Enter/exit:

```text
05 00 04 01 STATE
```

Pixel update:

```text
LENlo LENhi 05 01 ? R G B X0 Y0 X1 Y1 ...
```

In the firmware:

- RGB = offsets 5..7;
- `(x,y)` pairs follow from offset 8;
- valid coordinates: 0..15.

The byte at offset 4 is not yet semantically documented.

## Bulk transfers - CONFIRMED for GIF/RAW/TEXT

Implemented common header, 16 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | packet length |
| 2 | 1 | type |
| 3 | 1 | `00` |
| 4 | 1 | field not identified by the current parser |
| 5 | 4 | total payload size LE |
| 9 | 4 | payload CRC32 LE |
| 13..15 | 3 | header fields not yet documented |
| 16.. | - | payload chunk |

Implemented types:

| Type | Content |
|---:|---|
| `01` | GIF |
| `02` | RAW RGB 16x16 when size=768 |
| `03` | TEXT |

During an incomplete transfer:

```text
05 00 TYPE 00 01
```

On completion:

```text
05 00 TYPE 00 03
```

CRC32 is verified over the complete payload.

### RAW RGB 16x16 - CONFIRMED

Size:

```text
16 * 16 * 3 = 768 byte
```

Linear RGB pixel order. The firmware then maps logical coordinates to the physical serpentine matrix layout.

### GIF - CONFIRMED

The payload is a standard 16x16 GIF file (`GIF87a`/`GIF89a`). It is kept in RAM and decoded with AnimatedGIF, respecting delays, palettes and transparency.

### TEXT - CONFIRMED for the fields currently used

Global payload:

| Offset | Field |
|---:|---|
| 0 | glyph count |
| 1..3 | fields not yet documented |
| 4 | effect/movement |
| 5 | speed |
| 6 | color mode |
| 7 | text R |
| 8 | text G |
| 9 | text B |
| 10 | background mode |
| 11 | background R |
| 12 | background G |
| 13 | background B |

Each glyph is followed by a 20-byte record:

```text
7 byte META + 13 byte BITMAP
```

Each bitmap represents 13 rows x 8 columns. In the observed format, **bit 0 is the leftmost pixel**. This orientation was verified with non-symmetric text.

Observed example for `IW`:

```text
GLYPH 0
META   : 02 FF FF FF 00 00 00
BITMAP : 3E 08 08 08 08 08 08 08 08 08 3E 00 00

GLYPH 1
META   : 02 FF FF FF 00 00 00
BITMAP : 6B 2A 2A 2A 2A 2A 36 14 14 14 14 00 00
```

The complete semantics of the 7 META bytes are not yet decoded.

---

# Visual effects

## Effects command - CONFIRMED

```text
LENlo LENhi 03 02 EFFECT SPEED COUNT [R G B]...
```

| Field | Meaning |
|---|---|
| EFFECT | effect index |
| SPEED | speed |
| COUNT | number of colors |
| RGB | colors used |

The color channels observed in this command use an approximately **0..127** range; the firmware expands them to 0..255.

Seven visual effects observed in the app have been reproduced. Their appearance was refined by visual comparison with an app video; not every algorithm should be considered a mathematically exact copy of the original.

---

# Clock

## Style selection - CONFIRMED

```text
08 00 06 01 FLAGS R G B
```

Implemented interpretation:

```text
style     = FLAGS & 0x3F
24h       = FLAGS & 0x40
showDate  = FLAGS & 0x80
```

`R G B` is the color selected by the app; some styles also use their own graphic colors observed in the visual reference.

All 8 styles investigated during reverse engineering are implemented.

---

# Countdown

## Command - CONFIRMED, app compatibility PARTIAL

```text
07 00 08 80 MODE MIN SEC
```

`MIN` and `SEC` are converted to milliseconds.

Observed modes:

| MODE | Action |
|---:|---|
| `00` | reset |
| `01` | start with MIN:SEC value |
| `02` | pause |
| `03` | resume |

For start/pause/resume/reset the firmware currently returns standard ACK `01`.

At natural completion the firmware spontaneously sends:

```text
05 00 08 80 03
```

The local countdown logic works, but app UI compatibility is not yet considered complete.

---

# Stopwatch

## Command - CONFIRMED, original response UNKNOWN

```text
05 00 09 80 MODE
```

Modes:

| MODE | Action |
|---:|---|
| `00` | reset |
| `01` | start from zero |
| `02` | pause |
| `03` | resume |

The local state machine was verified: in one test, pausing after about 6 seconds produced an internal value of 6045 ms, resume continued from that value, and reset returned to zero.

The sniffer showed that the app **does not periodically poll** the timer. No additional commands are sent between START and PAUSE.

The firmware currently replies:

```text
05 00 09 80 01
```

but the app UI still does not behave as expected. `01/03` variants were also tested without success. The exact response behavior of the original device remains one of the main open questions.

---

# Scoreboard

## Command - CONFIRMED

```text
08 00 0A 80 A_lo A_hi B_lo B_hi
```

Both scores are little-endian `uint16` values.

---

# Audio / Rhythm

The app does not use a microphone on the emulated device: it sends the display data derived from phone audio.

**10 modes** were observed, split into 5 LEVEL and 5 FFT modes.

## LEVEL - CONFIRMED

```text
06 00 00 02 LEVEL MODE
```

- `MODE`: 1..5;
- `LEVEL`: instantaneous level, clamped to 0..12 by the renderer.

ACK:

```text
05 00 00 02 01
```

Observed visual sequence:

1. dancer/breakdance figure;
2. heart;
3. pseudo-spectrum with dotted frame;
4. face;
5. animated face/lips.

## FFT - CONFIRMED for the frame currently used

Logical frame of at least 21 bytes:

```text
21 00 01 02 MODE ...
```

- `MODE`: 0..4;
- the firmware uses 8 bands starting at offset 5;
- values are clamped to 0..12.

During captures, a 33-byte BLE write could contain one complete 21-byte frame plus the beginning of the next one; the parser treats the first 21 bytes as the logical frame.

ACK:

```text
05 00 01 02 01
```

Visual modes:

1. symmetric vertical bars from the center line;
2. similar to the previous mode, but color is associated with rows;
3. full-screen rainbow heart that contracts/expands with the bars;
4. spectrum from the vertical center line;
5. bars moving from top and bottom toward the center.

---

# Alarms

## Command - CONFIRMED for the implemented structure

Alarm packets use:

```text
CMD=00 SUB=80
```

The firmware provides 10 slots (`0..9`).

### Full packet

Header da 24 byte:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | total length LE |
| 2 | 1 | `00` |
| 3 | 1 | `80` |
| 4 | 1 | slot |
| 5 | 1 | flags |
| 6 | 1 | hour |
| 7 | 1 | minute |
| 8 | 1 | duration in seconds |
| 9 | 1 | reserved1 |
| 10 | 1 | contentType |
| 11 | 1 | buzzer |
| 12 | 1 | reserved2 |
| 13 | 4 | mediaSize LE |
| 17 | 4 | mediaCRC32 LE |
| 21 | 2 | reserved3 LE |
| 23 | 1 | mediaId |
| 24.. | - | media |

Media types confirmed in the firmware:

- `01` = GIF;
- `02` = RAW RGB.

Text content also appeared in alarm packets during testing; the full semantics of all `contentType` values still need dedicated captures.

### Weekday flags - CONFIRMED

The convention, also used by Schedules, is:

```text
bit 0 = enabled
bit 1 = Monday
bit 2 = Tuesday
bit 3 = Wednesday
bit 4 = Thursday
bit 5 = Friday
bit 6 = Saturday
bit 7 = Sunday
```

For a one-shot alarm the weekday bits may be zero; after execution the firmware clears the `enabled` bit.

### Short packet

If the packet is shorter than the full header, the firmware treats it as a metadata update/disable operation without rewriting media.

### ACK

```text
05 00 00 80 01
```

The buzzer is supported by both protocol and firmware but is not present on the current development hardware.

---

# Programs / Schedule

This is one of the best-verified parts of the protocol, thanks to programs containing 1, 3 and at least 12 activities.

## Global program state - CONFIRMED

```text
05 00 07 80 FLAGS
```

Verified interpretation:

```text
bit 0 = program enabled
bit 1 = sound enabled
```

Observed examples:

- `00` = disabled, sound off;
- `01` = enabled, sound off;
- `03` = enabled, sound on.

Required ACK:

```text
05 00 07 80 01
```

When a program is activated, the app sends activities one at a time and waits for each ACK.

## Activities - CONFIRMED

Format:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | total length LE |
| 2 | 1 | `05` |
| 3 | 1 | `80` |
| 4 | 1 | activity index |
| 5 | 1 | flags |
| 6 | 1 | start hour |
| 7 | 1 | start minute |
| 8 | 1 | end hour |
| 9 | 1 | end minute |
| 10 | 2 | contentType LE |
| 12 | 4 | payloadSize LE |
| 16 | 4 | CRC32 LE |
| 20 | 2 | reserved LE |
| 22 | 1 | mediaId |
| 23.. | - | payload |

Types:

```text
01 = GIF
02 = IMAGE / PNG 16x16
03 = TEXT
```

### Activity flags - CONFIRMED

```text
bit 0 = enabled
bit 1 = Monday
bit 2 = Tuesday
bit 3 = Wednesday
bit 4 = Thursday
bit 5 = Friday
bit 6 = Saturday
bit 7 = Sunday
```

Verified examples:

```text
03 = enabled + Monday
4B = enabled + Monday + Wednesday + Saturday
D5 = enabled + Tuesday + Thursday + Saturday + Sunday
C1 = enabled + Saturday + Sunday
A5 = enabled + Tuesday + Friday + Sunday
```

### Activity ACK - CONFIRMED AND CRITICAL

On success:

```text
05 00 05 80 03
```

This was verified experimentally. Replying with:

```text
05 00 05 80 01
```

the app reports an error even for a program containing a single activity and does not continue with later activities. With `03`, a 12-activity program is transferred completely.

The firmware uses `02` when validation fails; the original meaning of this status is not confirmed by a real device.

### Commit

No explicit end-of-list command was observed. The firmware therefore uses temporary staging and considers the upload complete after about 900 ms without new activities. It then atomically replaces the previous Schedule.

This is an emulator design choice, not a confirmed field or behavior of the original protocol.

### PNG

Observed Schedule images are 16x16, 8-bit, non-interlaced PNG files. The implemented decoder supports RGB/RGBA and standard PNG filters. Inflate uses `tinfl_decompressor` directly with state allocated on the heap: using the `tinfl_decompress_mem_to_mem()` wrapper caused a `loopTask` stack overflow on the ESP32 used for development.

---

# Open fields and behaviors

1. exact original-device response to stopwatch `09 80`;
2. complete general semantics of ACK statuses `01`, `02`, `03`;
3. meaning of several reserved bytes in Bulk, Alarm and Schedule headers;
4. complete meaning of the 7 META bytes for each TEXT glyph;
5. app commands/features not yet exercised;
6. exact mathematical correspondence of some visual/audio effects to the original firmware.

BUILD 62 records every unrecognized command and, when the OLED is enabled, immediately shows its length and first raw bytes on the diagnostic display. The OLED is pure event-driven to avoid disturbing matrix animation timing.

## Additional 32x32 findings (Builds 63-67)

### Device profile

The emulator successfully selected the application's 32x32 behavior by reporting screen type `0x03` in Device Info:

`09 00 01 80 04 0E 01 03 00`

The 16x16 profile uses screen type `0x01`.

Observed application behavior after selecting the 32x32 profile:

- cloud images/animations are different and use higher-resolution assets;
- Graffiti exposes and transmits the larger coordinate space;
- clock styles appear to use the same style identifiers and are rendered/scaled by the device;
- Programs/Schedules appear to be stored separately by the app for different screen profiles.

The last point is an application-side observation and is not yet known to be a protocol requirement.

### Text glyph sizes

For the 32x32 profile the application exposes text sizes 16 and 32.

Observed glyph record markers:

| Marker | Glyph bitmap | Bitmap bytes | Record size |
| --- | --- | ---: | ---: |
| `0x02` | 8x16 | 16 | 20 bytes |
| `0x05` | 16x32 | 64 | 68 bytes |

Each observed record consists of the marker, RGB color data and the rasterized glyph bitmap.

SimSun and SimHei do not appear to be transmitted as a device-side font selector. Changing the font family and resending identical text preserves the packet structure and glyph-size marker while changing the bitmap bytes. This strongly indicates that the mobile application rasterizes the selected font before transmission.

### 32x32 GIF chunking

Observed cloud GIF transfers on the simulated 32x32 profile used payload chunks of up to 4096 bytes. Examples included 30,321 bytes in 8 chunks and 14,571 bytes in 4 chunks.

Intermediate chunks were acknowledged with status `0x01`; transfer completion used status `0x03`. This further corroborates the interpretation:

- `0x01` = accepted / continue;
- `0x03` = transaction complete.
