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
- valid coordinates depend on the active logical profile: `0..MATRIX_WIDTH-1` and `0..MATRIX_HEIGHT-1` (therefore `0..15` only for the default 16x16 profile).

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

On transfer termination/completion:

```text
05 00 TYPE 00 03
```

For bulk transfers, the safest current interpretation is:

- `0x01` = intermediate/continue acknowledgement: the sender may continue the transaction;
- `0x03` = transaction terminated/completed: no further chunks are expected.

`0x03` must **not** be documented as a universal success code. BUILD 80 also uses it to close some failed transactions (for example after CRC or storage errors), while refusing to publish the invalid content. The exact original-device semantics of all ACK status values remain partially unresolved.

CRC32 is verified over the complete payload.

### Transfer and memory limits in BUILD 80

Several different limits coexist and must not be conflated:

- the bulk parser accepts declared transfers up to 10 MiB; this is a transport sanity limit, not a promise that every payload can be decoded;
- `MAX_GIF_SIZE` (128 KiB in BUILD 80) belongs to legacy/full-RAM GIF paths;
- normal BLE GIF uploads use LittleFS and are therefore governed primarily by filesystem capacity and decoder constraints rather than by `MAX_GIF_SIZE`;
- Alarm/Schedule stored-media playback can still use the legacy RAM path and is therefore constrained by contiguous heap availability.

These are implementation limits of the reference firmware, not confirmed limits of the iDotMatrix protocol.

### RAW RGB 16x16 - CONFIRMED

Size:

```text
16 * 16 * 3 = 768 byte
```

Linear RGB pixel order. The firmware then maps logical coordinates to the physical serpentine matrix layout.

### GIF - CONFIRMED

The payload is a standard GIF file (`GIF87a`/`GIF89a`). In BUILD 80, normal BLE GIF uploads are **not kept entirely in RAM**: chunks are streamed to LittleFS using alternating RX files, a completed RX file is promoted to the PLAY file, and AnimatedGIF is opened from the normal `loop()` with a fresh decoder instance for each media change. This is the stable path validated with 16x16, 32x32 and 64x64 app profiles.

**Important exception:** Alarm and Schedule media playback still contains a legacy RAM path that loads stored GIF media into `gifData`. Therefore BUILD 80 is not yet "streaming everywhere"; this is tracked as technical debt in `TODO.md`.

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

Observed BUILD 80 glyph records use a 4-byte metadata prefix followed by a bitmap whose size is selected by the marker:

| Marker | Status | Glyph | Metadata | Bitmap | Record |
|---:|---|---:|---:|---:|---:|
| `0x02` | observed/confirmed | 8x16 | 4 bytes | 16 bytes | 20 bytes |
| `0x05` | observed/confirmed | 16x32 | 4 bytes | 64 bytes | 68 bytes |
| `0x03` | compatibility alias in reference parser; not experimentally confirmed | 8x16 | 4 bytes | 16 bytes | 20 bytes |
| `0x06` | compatibility alias in reference parser; not experimentally confirmed | 16x32 | 4 bytes | 64 bytes | 68 bytes |

The older `7 META + 13 BITMAP` interpretation was superseded by the later 16x16/32x32 captures and must not be treated as the current protocol model. Bitmap orientation is handled by the reference renderer; `0x02` and `0x05` are the markers supported by direct experimental evidence.

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

The buzzer is supported by both protocol and firmware and is installed on the current development hardware as an active buzzer on GPIO18. The firmware drives it with a non-blocking trill pattern.

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

Current firmware records every unrecognized command and, when the OLED is enabled, immediately shows its length and first raw bytes on the diagnostic display. The OLED is pure event-driven to avoid disturbing matrix animation timing.

## Resolution profiles confirmed during emulator testing

| Device type | Resolution | Emulator observation |
|---|---:|---|
| `0x01` | 16x16 | original stable target |
| `0x03` | 32x32 | official app switches to 32x32 assets/features |
| `0x04` | 64x64 | official app switches to 64x64 cloud assets; GIF reception/playback verified |

The manufacturer/device profile byte is sufficient in our tests to make the official app select resolution-specific content. Graffiti coordinates and the general command structure remain compatible across the tested profiles.

### Text glyph sizes

Observed text records use marker `0x02` for an 8x16 glyph (16 bitmap bytes) and `0x05` for a 16x32 glyph (64 bitmap bytes). Changing SimSun/SimHei causes the app to resend different glyph bitmap data rather than sending a persistent font-selection command; therefore font rasterization is performed by the app.

### Large GIF transfers

32x32 and 64x64 cloud media continue to use 4096-byte bulk chunks. Intermediate transfer acknowledgement `0x01` and completion acknowledgement `0x03` remain consistent with earlier observations. During 64x64 tests, GIFs around 79 KB were successfully received and played.

The emulator now stores large media in LittleFS rather than requiring one contiguous RAM allocation. Receive and playback files are separate, and each media switch creates a fresh AnimatedGIF decoder instance. These are implementation requirements of this emulator, not claims about the original device firmware.



### Clock/date presentation (emulator behavior)

When the app enables date display, the emulator keeps the selected clock visual effect and alternates 30 seconds of `HH:MM` with 5 seconds of `DD/MM`. The date phase uses `/` rather than the clock `:` separator. This is emulator rendering behavior derived from the app option; exact timing/presentation on every original hardware model is not yet claimed.

The 16x16 stopwatch and countdown use a matching vertical layout with an animated timer icon above the numeric value. Stopwatch animation advances forward and remains white. Countdown remains white until the final five seconds, when it turns red.

## Reference implementation caveats (BUILD 80)

The Arduino firmware is a validated reverse-engineering reference, not a model for every future integration. Some blocking or callback-heavy operations remain, including a disconnect delay, a small main-loop delay, startup/OLED delays, and filesystem/bulk work performed from BLE callbacks. These are tolerated in the standalone experimental firmware but should **not** be copied into latency-sensitive integrations such as WLED. A WLED port should enqueue BLE work and perform filesystem, parsing and rendering operations from the normal WLED execution context.

The local LED brightness ceiling (`MAX_LED_BRIGHTNESS`, currently 50) is also a hardware/test configuration choice, not a protocol rule. An integration should map iDotMatrix brightness to the host application's configured brightness range.
