# iDotMatrix BLE Protocol - reverse-engineering notes

> Cross-project validation and model comparison: see [`docs/PROTOCOL-COMPARISON.md`](docs/PROTOCOL-COMPARISON.md).


This document describes the protocol observed between the iDotMatrix app and the ESP32 emulator.

The project began without an original device, using official-app traffic captures and differential tests. A physical original 64×64 iDotMatrix is now also used as a protocol oracle. Direct hardware observations are explicitly distinguished from emulator policy and inference.

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

The emulator advertising exposes the FA service and manufacturer data derived from observed devices. The resolution/profile byte depends on `IDOTMATRIX_SCREEN_TYPE`. Advertising fields are useful for discovery/profile behavior, but BUILD 108 testing showed that changing advertising version bytes alone does **not** change the MCU version shown by the official app.

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

## Device info and app-facing MCU version - CONFIRMED

### Advertising versus Device Info

The original 64×64 unit reports MCU `5.11` in the official app. BUILD 108 tested version-like bytes in advertising/manufacturer data, but changing those bytes did not change the MCU value displayed by the app.

BUILD 109 identified the controlling path: the 9-byte FA03 Device Info response. The emulator now encodes the public release major/minor there and keeps the internal `FW_BUILD` separate.

For `v0.4.0-dev`, response version bytes `00 04` were hardware-tested with the official app and are displayed as MCU **`0.04`**.

### Query

```text
04 00 01 80
```

### Observed/emulated response

```text
09 00 01 80 <releaseMajor> <releaseMinor> 01 <screenType> 00
```

The final profile byte identifies the configured matrix type. The release bytes are app-facing version information; the internal build number is intentionally not encoded in this two-byte field.

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

Implementation hardening in BUILD 82 validates the calendar fields before updating either clock: month must be 1-12, day must exist in that month (including leap-year handling), hour must be 0-23, and minute/second must be 0-59. A malformed synchronization packet is ignored but receives the same compatibility ACK as a valid one because an original-device negative/error ACK for this command has not yet been established.

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

The logic also supports time ranges crossing midnight. BUILD 85 validates `SH/SM`, `EH/EM` and `REDUCTION` before publishing a new ECO configuration. Invalid records are ignored while the existing ACK is preserved because an original-device error status is not yet known.

The configured reduction is re-evaluated once per second. This is an emulator runtime guard so static content reacts when an ECO interval boundary is crossed; the one-second polling policy is not claimed as observed original-device behavior. If optional RTC support is compiled in, an RTC reporting `lostPower()` is not accepted as a valid time source until BLE time synchronization updates it.

## Device reset - OBSERVED / EMULATOR POLICY

```text
04 00 03 80
```

Direct testing on an original 64×64 shows that reset removes the stored Device Assets content; the password remembered by the original device/app workflow is also cleared. BUILD 99 therefore promotes the emulator command from the earlier runtime-only reset to a destructive device-state reset.

The emulator clears transient renderers plus persisted Carousel, Alarm and Schedule media/metadata, stored brightness, ECO configuration and rotation. Starting with BUILD 101, an app-issued `03/80` is not treated as an electrical power cycle: the active BLE connection remains valid, the already synchronized volatile software clock is preserved, and the matrix remains logically ON with a black framebuffer ready for the next command. A real boot still follows the RTC -> stored Carousel -> screen-off policy. Password support is not currently implemented in BUILD 104, so there is no emulator password state to clear.

Clearing Alarm/Schedule and the additional emulator settings is an intentional, easy-to-explain reset policy; it is not yet claimed that the original hardware clears every one of those fields.

---

# Graphic content

## Password - PARTIALLY REVERSE-ENGINEERED / NOT IMPLEMENTED

Observed SET command for a six-digit password:

```text
08 00 04 02 enable pair01 pair23 pair45
```

The six decimal digits are encoded as three decimal-pair values, not ASCII or packed BCD. Example: `123456` -> decimal `12,34,56` -> bytes `0C 22 38`; `111111` -> `0B 0B 0B`. These SET frames were captured directly from the official app.

A VERIFY transaction is strongly indicated by original-hardware Android logcat and independent client-side reverse engineering:

```text
07 00 05 02 pair01 pair23 pair45
```

The Android capture showed two 7-byte GATT writes followed by 5-byte FA03 notifications during wrong/correct password submissions, and the app logged a cached `pwdByMac.<value>` entry. This strongly indicates per-device/MAC password caching in the app and a separate verification exchange. The captured logcat did not expose binary payload bytes, so the exact VERIFY response status semantics remain unconfirmed in this project.

BUILD 101-103 experimentally implemented SET/VERIFY and several ACK timing strategies. On hardware, the official app remained on the Set Password screen after SET, and no additional app command was observed. BUILD 104 therefore removes password handling from the emulator runtime. The framing and observations remain documented for future reverse engineering, but password support must not be described as implemented or compatible.

Direct testing on the original 64x64 unit also showed that device reset clears the stored password association/state. Whether command enforcement is performed fully by the original device, partly by the app, or by both remains open.

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
| 13..14 | 2 | GIF `timeSign` / Device Assets dwell seconds LE; observed in project captures and independently hardware-validated |
| 15 | 1 | GIF `imageIndex`; Device Assets slot `0..11` |
| 16.. | - | payload chunk |

Implemented types:

| Type | Content |
|---:|---|
| `01` | GIF |
| `02` | RAW RGB 16x16 when size=768 |
| `03` | TEXT |

BUILD 82 adds implementation-only inactivity guards: 5 seconds for an incomplete reconstructed logical packet and 30 seconds between Bulk packets for an active transaction. These values are emulator safety limits, not observed protocol timings. BUILD 88 fixes a mutex/timestamp-ordering regression introduced in BUILD 86 that could falsely trigger those guards on a valid multi-packet transfer; the timeout values themselves are unchanged. Aborted GIF transfers close and remove their partial RX file. TEXT transfers whose declared payload exceeds 4096 bytes are rejected rather than truncated.

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

`0x03` must **not** be documented as a universal success code. BUILD 82 (continuing the established behavior) also uses it to close some failed transactions (for example after CRC or storage errors), while refusing to publish the invalid content. The exact original-device semantics of all ACK status values remain partially unresolved.

CRC32 is verified over the complete payload.

### Transfer, filesystem and memory limits as of v0.4.0-dev / BUILD 99

Several different limits coexist and must not be conflated:

- the bulk parser accepts declared transfers up to 10 MiB; this is a transport sanity limit, not a promise that every payload can be decoded;
- normal BLE GIF uploads use LittleFS and are governed primarily by filesystem capacity and decoder constraints; BUILD 87 also rejects a GIF at transfer start when its declared payload is larger than the currently available LittleFS free space;
- BUILD 89 removes the active full-RAM compressed-GIF path: Alarm/Schedule GIF playback is also LittleFS-backed and no longer depends on one contiguous compressed-media allocation;
- Alarm and Schedule media are still embedded in a single reconstructed logical packet, so the current `MAX_PACKET_SIZE = 8192` imposes a separate practical transport ceiling (roughly 8168 bytes of Alarm media after its 24-byte header and 8169 bytes of Schedule payload after its 23-byte header);
- Alarm/Schedule GIF playback requires temporary filesystem capacity for a second copy of the selected GIF in `/event_play.gif`; this copy isolates the decoder from transactional source-file renames.
- TEXT has a 4096-byte payload ceiling. With the current canonical record sizes and the 64-glyph storage cap, this permits up to 64 8x16 glyphs or 60 16x32 glyphs in one TEXT payload.

These are implementation limits of the reference firmware, not confirmed limits of the iDotMatrix protocol.

BUILD 87 also changes storage failure policy: LittleFS is mounted without implicit formatting. `LITTLEFS_FORMAT_ON_MOUNT_FAIL=1` is an explicit emulator recovery option, disabled by default, and is not part of the iDotMatrix protocol. If storage is unavailable, filesystem-backed media operations fail while Preferences metadata remains readable.

### RAW RGB 16x16 - CONFIRMED

Size:

```text
16 * 16 * 3 = 768 byte
```

Linear RGB pixel order. The firmware then maps logical coordinates to the physical serpentine matrix layout.

### GIF - CONFIRMED

The payload is a standard GIF file (`GIF87a`/`GIF89a`). Since BUILD 87, normal BLE GIF uploads are **not kept entirely in RAM**: chunks are streamed to LittleFS using alternating RX files, a completed RX file is promoted to the PLAY file, and AnimatedGIF is opened from the normal `loop()` with a fresh decoder instance for each media change. This is the stable path validated with 16x16, 32x32 and 64x64 app profiles.

BUILD 89 removes the previous Alarm/Schedule exception. Before an Alarm/Schedule GIF starts, the emulator copies the persistent source file to `/event_play.gif`, verifies the copied stream against the stored size/CRC, and opens that disposable file through the LittleFS AnimatedGIF callbacks. The decoder therefore never holds an Alarm/Schedule transactional source file open while later configuration updates may rename it. `/event_play.gif` is implementation-only transient state and is deleted when playback stops and at boot.

### Device Assets carousel - OBSERVED / HARDWARE-TESTED, BUILD 97

The official app exposes three UI pages/lists of twelve positions, while current captures and independent original-hardware reverse engineering support a **single 12-slot device bank**. A pushed app page replaces device slots `0..11`.

Observed setup frame:

```text
11 00 02 01 0C 00 01 02 03 04 05 06 07 08 09 0A 0B
```

Best-supported interpretation:

```text
11 00        total frame length = 17
02 01        Device Assets slot setup / material setup
0C           number of slot IDs carried = 12
00..0B       device slot IDs 0..11
```

A three-item page still sends all twelve slot IDs, so `0x0C` is not the number of occupied carousel entries.

Bulk header fields observed in project captures:

```text
bytes 13..14 : timeSign, uint16 little-endian dwell seconds
byte 15      : imageIndex / Device Assets slot
```

Captures directly confirm `timeSign=5` and `timeSign=30` with `imageIndex=0,1,2`. A later 12-position capture additionally shows a `DataType.TEXT` Bulk between GIF `imageIndex=4` and GIF `imageIndex=6`. In BUILD 94 that live TEXT parser called the normal display-mode transition and unintentionally closed the Device Assets upload context; subsequent GIFs 6..11 were then misclassified as live GIFs. BUILD 95 treats a TEXT Bulk carrying a carousel-range `imageIndex` during an open Device Assets push as a stored slot instead. Mixed GIF/TEXT playback has since been hardware-tested successfully; the exact captured TEXT index/dwell remains worth recording explicitly in a future trace.

Current emulator behavior (BUILD 95 model, BUILD 96 blackout, consolidated in BUILD 97):

1. `0A/01` sets Device Assets view intent. Captures show it may be sent before a later page push and is not required as a post-upload terminator.
2. `02/01` validates the slot descriptor, stops current carousel playback, clears the declared slots, preserves any previously established Assets-view intent, opens replacement, and forces the physical LED output black (introduced and hardware-validated in BUILD 96) without modifying `screenOn` or the logical framebuffer.
3. GIF (`type=1`) and observed TEXT (`type=3`) Bulk transfers with `imageIndex=0..11` are stored in the corresponding slot without changing the visible display during the push.
4. Slot metadata persists content type, `timeSign`, size and CRC. GIF files and TEXT payload files are kept separately.
5. Because no explicit end-of-push frame is present in the short-page captures, an already-requested Assets view resumes after **3000 ms** with no active Bulk and no newly committed carousel asset. The implementation always releases the temporary output blackout when the replacement settles. If Assets view is active, the first stored slot starts; otherwise the preserved framebuffer is restored. The settle timeout and blackout are emulator policies, not inferred original-device protocol behavior.
6. GIF slots use AnimatedGIF; TEXT slots use the existing TEXT parser/renderer while preserving carousel state.
7. Empty/unconfigured slots are skipped.
8. Repeated `0A/01` while already active is idempotent; if replacement is open it only preserves view intent and does not expose the partial bank.
9. Live/Cloud content outside an open Device Assets replacement retains the stable v0.3.1 path.
10. Hardware testing confirms that BLE/app disconnect does not terminate an already-running carousel; playback continues autonomously on the emulator.

Independent original-hardware reverse engineering corroborates the 12-slot bank, GIF slot storage, `timeSign`, `imageIndex` and autonomous playback. Its published notes currently state that persistence requires GIF data. Mixed GIF/TEXT carousel playback is **project-observed and hardware-tested on this ESP32 emulator**, but persistence/playback of TEXT assets on original iDotMatrix hardware remains unverified and is not promoted to a universal protocol rule.

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

Observed glyph records used by the BUILD 86 reference parser use a 4-byte metadata prefix followed by a bitmap whose size is selected by the marker:

| Marker | Status | Glyph | Metadata | Bitmap | Record |
|---:|---|---:|---:|---:|---:|
| `0x02` | observed/confirmed | 8x16 | 4 bytes | 16 bytes | 20 bytes |
| `0x05` | observed/confirmed | 16x32 | 4 bytes | 64 bytes | 68 bytes |
| `0x03` | compatibility alias in reference parser; not experimentally confirmed | 8x16 | 4 bytes | 16 bytes | 20 bytes |
| `0x06` | compatibility alias in reference parser; not experimentally confirmed | 16x32 | 4 bytes | 64 bytes | 68 bytes |

The older `7 META + 13 BITMAP` interpretation was superseded by the later 16x16/32x32 captures and must not be treated as the current protocol model. Bitmap orientation is handled by the reference renderer; `0x02` and `0x05` are the markers supported by direct experimental evidence.

The canonical record boundaries are therefore:

```text
0x02 record: 4 metadata bytes (marker included) + 16 bitmap bytes = 20 bytes
0x05 record: 4 metadata bytes (marker included) + 64 bitmap bytes = 68 bytes
```

The complete semantics of all metadata bytes after the marker are not yet decoded. Historical examples that split a record as `7 META + 13 BITMAP` are obsolete and must not be used to implement new parsers.

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

Starting with BUILD 98, natural completion also triggers a local one-shot active-buzzer notification: three short 90 ms pulses separated by 70 ms gaps. This does not add or alter any BLE packet and is documented as emulator-side behavior rather than an observed original-device protocol requirement. Countdown reset or a new Countdown start cancels a completion trill still in progress.

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

### BUILD 83 persistence/preemption hardening

Alarm configuration is still interpreted from the same observed packet shape, but the emulator now stages a candidate slot before publishing it. Hour/minute fields are sanity-checked; full-media updates write to a temporary file, preserve the previous media as a backup, and update the live slot only after media replacement and Preferences persistence succeed. The compatibility ACK is intentionally unchanged because an original-device Alarm storage-failure status has not been established. At runtime Alarm has explicit priority over Schedule: an active Schedule is stopped/restored before Alarm media is loaded. These transaction and priority rules are emulator implementation behavior, not claims about original hardware.


## Command - CONFIRMED for the implemented structure

Alarm packets use:

```text
CMD=00 SUB=80
```

The firmware provides 10 slots (`0..9`).

### Full packet

24-byte header:

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

The buzzer is supported by both protocol and firmware and is installed on the current development hardware as an active buzzer on GPIO18. Direct original-hardware testing shows Alarm uses repeating three-beep trills and Program/Schedule uses the same repeating pattern for roughly 30 seconds; original Countdown completion is silent. Emulator BUILD 99 intentionally keeps Alarm repeating but uses a single three-beep trill for Program/Schedule. BUILD 98 introduced a single three-beep Countdown-completion trill as an emulator convenience. These local buzzer policies add no BLE packet.

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

No explicit end-of-list command was observed. The firmware therefore uses temporary staging and considers the upload complete after about 900 ms without new activities. It then commits the staged Schedule. In BUILD 83 this remains an emulator implementation strategy rather than an observed protocol rule. The commit now preflights staged media by size/CRC, backs up the previous media set, promotes the new files, persists Preferences metadata, and only then publishes the new runtime Schedule. Failures use best-effort rollback. On boot, destination/backup media are reconciled against the metadata that actually persisted. This reduces partial-update risk but is **not claimed to be a filesystem-wide ACID transaction**, especially across unexpected power loss.

This is an emulator design choice, not a confirmed field or behavior of the original protocol.

### PNG

Observed Schedule images are 16x16, 8-bit, non-interlaced PNG files. The implemented decoder supports RGB/RGBA and standard PNG filters. Inflate uses `tinfl_decompressor` directly with state allocated on the heap: using the `tinfl_decompress_mem_to_mem()` wrapper caused a `loopTask` stack overflow on the ESP32 used for development.

---

# Open fields and behaviors

1. exact original-device response to stopwatch `09 80`;
2. complete general semantics of ACK statuses `01`, `02`, `03`;
3. meaning of several reserved bytes in Bulk, Alarm and Schedule headers;
4. complete meaning of the currently observed 4-byte TEXT metadata prefix (marker included), especially the bytes after the marker;
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

## Reference implementation caveats (v0.3.1 / BUILD 90)

The Arduino firmware is a validated reverse-engineering reference, not a model for every future integration. BUILD 86 serializes the shared FA02/server-callback runtime state with the Arduino loop through a FreeRTOS task mutex and defers the historical 300 ms advertising restart out of the disconnect callback. This removes known cross-core data races on those shared fields without using an interrupt-disabled critical section.

Some callback-heavy operations still remain, especially filesystem/Bulk processing performed from FA02 writes, together with a small main-loop delay and startup/OLED delays. These are tolerated in the standalone experimental firmware but should **not** be copied into latency-sensitive integrations such as WLED. A WLED port should enqueue BLE work and perform filesystem, parsing and rendering operations from the normal WLED execution context.

The local LED brightness ceiling (`MAX_LED_BRIGHTNESS`, currently 50) is also a hardware/test configuration choice, not a protocol rule. An integration should map iDotMatrix brightness to the host application's configured brightness range.

## Original 64×64 hardware observations — BUILD 99

The following behaviors were directly observed on an original iDotMatrix 64×64 and are reference evidence, not requirements that the emulator must copy when a better local policy is intentional.

| Area | Original hardware observation | Emulator policy |
|---|---|---|
| Boot | Stored Device Assets carousel resumes after power cycle | Same fallback when no valid optional RTC exists |
| RTC/time | No persistent RTC observed; Alarm/Program need a new time sync after reboot | Optional RTC supported; valid RTC has boot priority |
| Alarm buzzer | Repeating three-beep trill | Repeating three-beep trill |
| Program buzzer | Same trill repeated for about 30 s | One three-beep trill only |
| Countdown buzzer | Silent | One three-beep trill (intentional enhancement) |
| Power Saving | Reduces brightness | Implemented |
| Flip | 180-degree display rotation | Implemented |
| Cloud GIF/Graffiti | Leaving the app section restores background content | Emulator intentionally keeps the selected content visible |
| Reset `03/80` | Clears stored Device Assets; stored password association/state is also cleared by the observed workflow | Full emulator-managed state reset; password runtime is not implemented in BUILD 104 |
| Device information | Tested 64×64 reports MCU `5.11` | Version-query response not yet implemented |

### Time model

After a valid app time synchronization the original unit continues Alarm execution after BLE disconnect while power remains applied. After a power cycle the previously configured Alarm/Program cannot execute until time is synchronized again. This is consistent with a volatile software clock and no persistent RTC.

### Reset policy

BUILD 99 introduced destructive clearing of emulator-managed persistent state. BUILD 101 refined the live behavior of `03/80`: Carousel, Alarm and Schedule media/metadata, stored brightness, ECO and rotation are cleared, but the already synchronized volatile software clock is preserved because the BLE session is still alive. The matrix remains logically ON and black, ready for the next app command. A real power cycle remains distinct and follows the normal boot policy. BUILD 104 removes the unverified emulator password runtime, so reset currently has no password state to clear.



## Historical password timing experiment

BUILD 101-103 explored password SET/VERIFY acknowledgements and timing, but those runtime experiments were removed in BUILD 104 because the official app never completed the SET-password flow. Source review also showed that the nominal BUILD 103 “deferred main-loop” timing was not implemented as independently as originally described. Therefore those builds are historical experiments only and must not be treated as protocol evidence.

Current firmware does **not** implement password SET/VERIFY runtime behavior. The observed packet framing, decimal-pair encoding, app-side `pwdByMac` caching, and 7-byte-write / 5-byte-notification evidence remain documented as partial reverse-engineering findings.
