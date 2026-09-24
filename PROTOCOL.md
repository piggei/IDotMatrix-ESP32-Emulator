# iDotMatrix BLE Protocol - reverse-engineering notes

> **Rendering note:** TEXT wire format is independent from the current multiline renderer. For non-scrolling effects the emulator packs rasterized glyphs into all rows allowed by the logical display/font height and vertically centers the rows actually used. Scrolling modes remain single-line. This is renderer behavior, not a new BLE field or command.

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

The emulator advertising exposes the FA service and manufacturer data derived from observed devices. The resolution/profile byte depends on `IDOTMATRIX_SCREEN_TYPE`. Advertising fields are useful for discovery/profile behavior, but hardware testing showed that changing advertising version bytes alone does **not** change the MCU version shown by the official app.

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

For Schedules the distinction is critical and depends on whether the activity media object is complete:

```text
07 80 -> ACK 01
05 80 incomplete large activity chunk -> ACK 01
05 80 final / one-packet CRC-valid activity -> ACK 03
```

Small-activity tests established that a complete one-packet `05/80` must receive `03`; replying with `01` there makes the app report an error. Hardware captures on the 64x64 path established the complementary rule for large media: `01` is required while more chunks are expected, and `03` is terminal/final.

The universal semantics of `01/02/03` outside these observed command families are not yet considered fully decoded.

---

## Original-hardware HCI command cross-check - 2026-09-20

The Android Bluetooth HCI snoop captured against an original 64x64 iDotMatrix also cross-checks several ordinary FA02/FA03 command families. These are useful because they confirm the packet framing and acknowledgement behavior on physical hardware rather than only against the emulator. The examples below are exact application payloads observed on FA02 with the corresponding FA03 notification where one followed.

| Function | App -> original device | Original device -> app | Evidence note |
|---|---|---|---|
| Time sync | `0B 00 01 80 1A 09 14 07 0F 13 16` | `05 00 01 80 01` | Standard ACK observed; the device also emitted its 9-byte Device Info notification during the same connection setup sequence. |
| Enter DIY/Graffiti | `05 00 04 01 01` | `05 00 04 01 01` | Repeatedly observed. |
| Leave DIY/Graffiti | `05 00 04 01 00` | `05 00 04 01 01` | Repeatedly observed. |
| Clock style/color | `08 00 06 01 C0 61 18 D6` | `05 00 06 01 01` | Confirms the `06/01` clock family and standard ACK. |
| Device Assets view/start | `04 00 0A 01` | `05 00 0A 01 01` | Confirms the `0A/01` Assets-view control packet. |
| Device Assets slot setup | `11 00 02 01 0C 00 01 02 03 04 05 06 07 08 09 0A 0B` | `05 00 02 01 01` | Confirms the 12-slot setup framing used before media pushes. |
| Countdown start | `07 00 08 80 01 00 0F` | `05 00 08 80 01` | Example starts a 15-second countdown. |
| Countdown reset | `07 00 08 80 00 00 00` | `05 00 08 80 01` | Reset uses the same normal ACK. |
| Preset/Default activation | `0A 00 06 02 05 0E 0F 10 11 12` | `05 00 06 02 01` | Confirms ordered activation of five uploaded Preset slots. |
| Program global state | `05 00 07 80 02` / `05 00 07 80 03` | `05 00 07 80 01` | Both observed in the original-hardware session. |
| Display power OFF | `05 00 07 01 00` | `05 00 07 01 01` | Confirms screen-power family. |
| Display power ON | `05 00 07 01 01` | `05 00 07 01 01` | Confirms screen-power family. |

The same trace also confirms Schedule activity acknowledgement behavior already described below: `05/80` receives status `01` while multipart activity media are incomplete and status `03` when an activity object is complete. Large media writes in the HCI log are transport fragments and are **not** documented as standalone commands.

The reduced evidence excerpt is stored in [`docs/captures/15-original-hardware-command-crosscheck.txt`](docs/captures/15-original-hardware-command-crosscheck.txt).

---

# General commands

## Device info and app-facing MCU version - CONFIRMED

### Advertising versus Device Info

The original 64×64 unit reports MCU `5.11` in the official app. Earlier experiments changed version-like bytes in advertising/manufacturer data, but those changes did not alter the MCU value displayed by the app.

Hardware testing identified the controlling path: the 9-byte FA03 Device Info response. The emulator encodes the public release major/minor there and keeps the internal `FW_BUILD` separate.

For the 0.4.x protocol baseline, response version bytes `00 04` were hardware-tested with the official app and are displayed as MCU **`0.04`**.

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

The firmware uses this synchronization as the base for its software clock. When the DS3231 backend is enabled (`IDOTMATRIX_RTC_TYPE=IDOTMATRIX_RTC_DS3231`) and `IDOTMATRIX_RTC_SYNC_FROM_BLE=1`, the same command also updates the hardware RTC and clears its oscillator-stop condition.

The emulator validates the calendar fields before updating either clock: month must be 1-12, day must exist in that month (including leap-year handling), hour must be 0-23, and minute/second must be 0-59. A malformed synchronization packet is ignored but receives the same compatibility ACK as a valid one because an original-device negative/error ACK for this command has not yet been established.

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

The logic also supports time ranges crossing midnight. The emulator validates `SH/SM`, `EH/EM` and `REDUCTION` before publishing a new ECO configuration. Invalid records are ignored while the existing ACK is preserved because an original-device error status is not yet known.

The configured reduction is re-evaluated once per second. This is an emulator runtime guard so static content reacts when an ECO interval boundary is crossed; the one-second polling policy is not claimed as observed original-device behavior. If the DS3231 backend is enabled, a set oscillator-stop flag or invalid stored date/time is not accepted as a valid time source until BLE time synchronization updates the RTC.

## Device reset - OBSERVED / EMULATOR POLICY

```text
04 00 03 80
```

Direct testing on an original 64×64 shows that reset removes the stored Device Assets content; the password remembered by the original device/app workflow is also cleared. The emulator therefore treats this command as a destructive device-state reset rather than the earlier runtime-only reset.

The emulator clears transient renderers plus persisted Carousel, Alarm and Schedule media/metadata, the volatile Preset/Default bank, stored brightness, ECO configuration and rotation. An app-issued `03/80` is not treated as an electrical power cycle: the active BLE connection remains valid, the already synchronized volatile software clock is preserved, and the matrix remains logically ON with a black framebuffer ready for the next command. A real boot follows the stored Carousel -> valid RTC Clock -> screen-off policy. Password support is not currently implemented, so there is no emulator password state to clear.

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

Historical development builds experimentally implemented SET/VERIFY and several ACK timing strategies. On hardware, the official app remained on the Set Password screen after SET, and no additional app command was observed. The release firmware therefore does not implement password handling in the emulator runtime. The framing and observations remain documented for future reverse engineering, but password support must not be described as implemented or compatible.

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
| `02` | RAW RGB when declared size equals the active logical framebuffer size |
| `03` | TEXT |

The emulator uses implementation-only inactivity guards: 5 seconds for an incomplete reconstructed logical packet and 30 seconds between Bulk packets for an active transaction. These values are emulator safety limits, not observed protocol timings. A later fix corrected a mutex/timestamp-ordering regression that could falsely trigger those guards on a valid multi-packet transfer; the timeout values themselves are unchanged. Aborted GIF transfers close and remove their partial RX file. TEXT transfers whose declared payload exceeds `MAX_TEXT_PAYLOAD` (currently 16654 bytes) are rejected rather than truncated.

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

`0x03` must **not** be documented as a universal success code. The emulator also uses it to close some failed transactions (for example after CRC or storage errors), while refusing to publish the invalid content. The exact original-device semantics of all ACK status values remain partially unresolved.

CRC32 is verified over the complete payload.

### Current emulator transfer, filesystem and memory limits

Several different limits coexist and must not be conflated:

- the bulk parser accepts declared transfers up to 10 MiB; this is a transport sanity limit, not a promise that every payload can be decoded;
- normal BLE GIF uploads use LittleFS and are governed primarily by filesystem capacity and decoder constraints; the emulator also rejects a GIF at transfer start when its declared payload is larger than the currently available LittleFS free space;
- the active full-RAM compressed-GIF path has been removed: Alarm/Schedule GIF playback is also LittleFS-backed and no longer depends on one contiguous compressed-media allocation;
- Hardware captures show that 64x64 Alarm media can span multiple complete FA02 logical packets. Each chunk repeats the 24-byte Alarm header; `mediaSize`/`mediaCRC` describe the complete object. The per-logical-packet `MAX_PACKET_SIZE = 8192` therefore does not limit the total Alarm media size to one packet. Hardware captures confirm the corresponding large Program/Schedule behavior: each `05/80` logical packet repeats the 23-byte activity framing, offset 10 is the one-byte media type, offset 11 is observed as `0x00` on the first packet and `0x02` on continuations, incomplete chunks receive ACK `0x01`, and the CRC-valid final chunk receives `0x03`;
- Alarm/Schedule GIF playback requires temporary filesystem capacity for a second copy of the selected GIF in `/event_play.gif`; this copy isolates the decoder from transactional source-file renames.
- TEXT uses `MAX_TEXT_PAYLOAD = 16654` bytes (14-byte global header plus up to 64 records of 4-byte metadata + 256-byte bitmap). The current storage cap is 64 glyphs for the supported 8x16, 16x32 and 32x64 raster families.

These are implementation limits of the reference firmware, not confirmed limits of the iDotMatrix protocol.

LittleFS mount/format behavior is emulator policy rather than protocol behavior. The current source enables `LITTLEFS_FORMAT_ON_MOUNT_FAIL=1` for first-use/recovery convenience: it first attempts a normal mount, then formats and retries only after mount failure. MatrixPortal S3 still requires a partition table containing a SPIFFS/LittleFS-compatible data partition; a FAT-only layout cannot be repaired by formatting through LittleFS. If storage remains unavailable, filesystem-backed media operations fail while independent Preferences metadata can remain readable.

### Graffiti full-raster type `00` - CONFIRMED ON ORIGINAL 64x64 HARDWARE

A Bluetooth HCI capture made while the official Android app sent a 64x64 Graffiti image to original iDotMatrix hardware established a dedicated raster transport that is **not** the normal 16-byte Bulk format.

Logical packet header:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | logical packet length, little-endian |
| 2 | 1 | type `0x00` |
| 3 | 1 | `0x00` in the captured transfer |
| 4 | 1 | chunk marker: `0x00` first, `0x02` continuation |
| 5 | 4 | complete raster size, little-endian |
| 9 | - | RGB payload |

For the captured 64x64 image:

```text
total raster size = 12288 = 64 * 64 * 3
chunk payload      = 4096 bytes
chunk count        = 3
```

Observed original-hardware acknowledgement sequence:

```text
first 4096 bytes   -> 05 00 00 00 02
second 4096 bytes  -> 05 00 00 00 02
final 4096 bytes   -> 05 00 00 00 01
```

The transport does not expose the CRC32 field used by normal 16-byte Bulk frames. The emulator therefore keeps a dedicated Graffiti raster state machine and publishes the image only when the declared raster byte count is complete.

### RAW RGB 16x16 - CONFIRMED

Size:

```text
16 * 16 * 3 = 768 byte
```

Linear RGB pixel order. The firmware then maps logical coordinates to the physical serpentine matrix layout.

### GIF - CONFIRMED

The payload is a standard GIF file (`GIF87a`/`GIF89a`). Normal BLE GIF uploads are **not kept entirely in RAM**: chunks are streamed to LittleFS using alternating RX files, a completed RX file is promoted to the PLAY file, and AnimatedGIF is opened from the normal `loop()` with a fresh decoder instance for each media change. This is the stable path validated with 16x16, 32x32 and 64x64 app profiles.

The current implementation removes the previous Alarm/Schedule exception. Before an Alarm/Schedule GIF starts, the emulator copies the persistent source file to `/event_play.gif`, verifies the copied stream against the stored size/CRC, and opens that disposable file through the LittleFS AnimatedGIF callbacks. The decoder therefore never holds an Alarm/Schedule transactional source file open while later configuration updates may rename it. `/event_play.gif` is implementation-only transient state and is deleted when playback stops and at boot.

### Device Assets carousel - OBSERVED / HARDWARE-TESTED

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

Captures directly confirm `timeSign=5` and `timeSign=30` with `imageIndex=0,1,2`. A later 12-position capture additionally shows a `DataType.TEXT` Bulk between GIF `imageIndex=4` and GIF `imageIndex=6`. An earlier implementation let the live TEXT parser call the normal display-mode transition, unintentionally closing the Device Assets upload context; subsequent GIFs 6..11 were then misclassified as live GIFs. The current implementation treats a TEXT Bulk carrying a carousel-range `imageIndex` during an open Device Assets push as a stored slot instead. Mixed GIF/TEXT playback has since been hardware-tested successfully; the exact captured TEXT index/dwell remains worth recording explicitly in a future trace.

Current emulator behavior:

1. `0A/01` sets Device Assets view intent. Captures show it may be sent before a later page push and is not required as a post-upload terminator.
2. `02/01` validates the slot descriptor, stops current carousel playback, clears the declared slots, preserves any previously established Assets-view intent, opens replacement, and forces the physical LED output black (hardware-validated behavior) without modifying `screenOn` or the logical framebuffer.
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

Observed glyph records used by the current reference parser use a 4-byte metadata prefix followed by a bitmap whose size is selected by the marker:

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

Natural completion also triggers a local one-shot buzzer notification: three short 90 ms pulses separated by 70 ms gaps. This does not add or alter any BLE packet and is documented as emulator-side behavior rather than an observed original-device protocol requirement. Countdown reset or a new Countdown start cancels a completion trill still in progress.

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

## FFT - CONFIRMED

Each logical FFT frame is exactly 21 bytes:

```text
21 00 01 02 MODE B0 B1 ... B15
```

- `MODE`: 0..4;
- bytes 5..20 carry 16 wire bands;
- values are clamped to 0..12;
- the legacy renderer uses 8 logical bands obtained by averaging adjacent wire-band pairs (`B0+B1`, `B2+B3`, ...).

BLE ATT writes do not define FFT-frame boundaries. Captures show that a 33-byte write can contain one complete 21-byte frame plus the first 12 bytes of the following frame. The audio byte-stream reassembler preserves that remainder until the next write completes the frame.

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

### Persistence/preemption hardening

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

### Multi-packet Alarm media observation

On the 64x64 profile, Alarm media has been observed split across more than one complete FA02 logical packet. Every logical packet repeats the 24-byte Alarm header. `mediaSize` and `mediaCRC32` remain the size and CRC of the **complete** media object, while bytes from offset 24 onward are only the current chunk.

A captured 6706-byte GIF was delivered as 4096 bytes followed by 2610 bytes. `reserved2` was `0x00` on the first packet and `0x02` on the following packet. The exact protocol meaning of `reserved2` is still not claimed; completion is determined from accumulated byte count plus the complete-object CRC.

The emulator stages these chunks in LittleFS and publishes the new Alarm media only after the declared byte count and CRC are both valid.

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

The protocol carries buzzer/sound state, and the emulator includes optional active and passive buzzer backends behind compile-time hardware configuration. Passive buzzers use hardware LEDC tone generation; generic source defaults keep buzzer output disabled unless a target enables it. Direct original-hardware testing shows Alarm uses repeating three-beep trills and Program/Schedule uses the same repeating pattern for roughly 30 seconds; original Countdown completion is silent. The emulator policy keeps Alarm repeating, uses one three-beep trill for Program/Schedule, and optionally emits one three-beep Countdown-completion trill when buzzer hardware is enabled. These local buzzer policies add no BLE packet.

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
| 10 | 1 | contentType |
| 11 | 1 | per-chunk marker; observed `00` on first chunk, `02` on continuation chunks |
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

For a complete one-packet activity, or the final CRC-valid chunk of a large activity:

```text
05 00 05 80 03
```

For an accepted but incomplete large activity chunk:

```text
05 00 05 80 01
```

The two observations are complementary. Small-activity tests showed that `01` is wrong when the activity is already complete: the app reports an error and does not proceed normally. Large-media hardware tests showed that `03` is wrong while more bytes remain: after the first ~4096-byte chunk the app does not continue the same media object. Returning `01` for incomplete chunks makes the 64x64 app send the remaining chunks, while the final chunk still receives `03`.

The firmware uses `02` when validation fails; the original meaning of this status is not confirmed by a real device.

### Commit

No explicit end-of-list command was observed. The firmware therefore uses temporary staging and considers the upload complete after about 900 ms without new activities. It then commits the staged Schedule. This remains an emulator implementation strategy rather than an observed protocol rule. The commit now preflights staged media by size/CRC, backs up the previous media set, promotes the new files, persists Preferences metadata, and only then publishes the new runtime Schedule. Failures use best-effort rollback. On boot, destination/backup media are reconciled against the metadata that actually persisted. This reduces partial-update risk but is **not claimed to be a filesystem-wide ACID transaction**, especially across unexpected power loss.

This is an emulator design choice, not a confirmed field or behavior of the original protocol.

### Large Schedule media continuation - HARDWARE CHARACTERIZED

The emulator does not assume that one Schedule activity's complete media object fits behind the 23-byte `05/80` header in a single reconstructed logical packet. The 64x64 app is confirmed to repeat that framing across many chunks, with `mediaSize` and CRC32 describing the complete media object.

Hardware validation established the flow-control rule: an incomplete accepted chunk must receive status `0x01`; the final CRC-valid chunk receives `0x03`. In the captured 48,456-byte image transfer the app sent exactly 48,456 payload bytes.

The same capture exposed a header-identity issue. Emulator totals advanced `4096 -> 4096 -> 8192 ... -> 44360/48456`. Since the payload transmitted by the app summed exactly to the declared 48,456 bytes, the second logical packet had caused the emulator to discard the already staged first 4096 bytes. The reset path can only occur when one of the activity-header fields used for transaction matching changes between chunks.

An in-progress continuation is therefore identified by activity index plus the immutable media-object tuple `contentType + mediaSize + mediaCRC`. Day/time fields, `reserved`, and `mediaId` are retained from the first chunk but are not allowed to reset the media staging file while that object identity remains unchanged.

Hardware validation identified the actual changing byte. The first chunk decoded as `t=1`, while continuation chunks decoded as `t=513`; in hexadecimal this is exactly `0x0001 -> 0x0201`. Size, CRC and the other logged activity metadata remained unchanged. The corrected field boundary is: offset 10 is the one-byte `contentType`, and offset 11 is separate per-chunk framing metadata. It is observed as `0x00` on the first chunk and `0x02` on following chunks. The firmware logs it separately as `m=0x..` and does not use it as media identity. The exact semantic name of that marker is still unconfirmed.

A partial activity suppresses the 900 ms list commit. If the media transaction times out, its media identity changes, or validation fails, the new staging transaction is discarded/restarted without publishing a partial Schedule; the previously active committed Schedule remains untouched.

### PNG

Observed Schedule images are 16x16, 8-bit, non-interlaced PNG files. The implemented decoder supports RGB/RGBA and standard PNG filters. Inflate uses `tinfl_decompressor` directly with state allocated on the heap: using the `tinfl_decompress_mem_to_mem()` wrapper caused a `loopTask` stack overflow on the ESP32 used for protocol validation.

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

When date display is enabled, 16x16 and 32x32 retain the established alternating presentation: roughly 30 seconds of `HH:MM` followed by 5 seconds of `DD/MM`, with `/` used for the date separator. On native 64x64, Clock styles 0 and 3 instead render `HH:MM` and `DD/MM` simultaneously as two independently centered rows. Style 0 retains the animated rainbow border; style 3 retains its selected-color background with black text. A short entry-protection window preserves an already-enabled date preference against transient `showDate=0` packets emitted by the app while entering or changing Clock styles.

Countdown uses the captured original-device hourglass composition: white glass/frame, brown bases and orange sand. Minutes are white; seconds are orange and turn red only during the final ten seconds. At `00:00` the final hourglass frame remains frozen. Stopwatch uses the reconstructed white/gray-lilac dial, orange button/seconds and red elapsed-time-driven hand. Countdown and Stopwatch colons blink at 1 Hz.

## Reference implementation caveats

The Arduino firmware is a validated reverse-engineering reference, not a model for every future integration. The current implementation serializes the shared FA02/server-callback runtime state with the Arduino loop through a FreeRTOS task mutex and defers the historical 300 ms advertising restart out of the disconnect callback. This removes known cross-core data races on those shared fields without using an interrupt-disabled critical section.

Some callback-heavy operations still remain, especially filesystem/Bulk processing performed from FA02 writes, together with a small main-loop delay and startup/OLED delays. These are tolerated in the standalone experimental firmware but should **not** be copied into latency-sensitive integrations such as WLED. A WLED port should enqueue BLE work and perform filesystem, parsing and rendering operations from the normal WLED execution context.

The local LED brightness ceiling (`MAX_LED_BRIGHTNESS`, currently 50) is also a hardware/test configuration choice, not a protocol rule. An integration should map iDotMatrix brightness to the host application's configured brightness range.

## Original 64×64 hardware observations

The following behaviors were directly observed on an original iDotMatrix 64×64 and are reference evidence, not requirements that the emulator must copy when a better local policy is intentional.

| Area | Original hardware observation | Emulator policy |
|---|---|---|
| Boot | Stored Device Assets carousel resumes after power cycle | Stored Carousel resumes first; valid DS3231 starts Clock only when no Carousel can start |
| RTC/time | No persistent RTC observed; Alarm/Program need a new time sync after reboot | DS3231 backend can persist time and provides Clock fallback when no stored Carousel starts |
| Alarm buzzer | Repeating three-beep trill | Repeating three-beep trill |
| Program buzzer | Same trill repeated for about 30 s | One three-beep trill only |
| Countdown buzzer | Silent | One three-beep trill (intentional enhancement) |
| Power Saving | Reduces brightness | Implemented |
| Flip | 180-degree display rotation | Implemented |
| Cloud GIF/Graffiti | Leaving the app section restores background content | Emulator intentionally keeps the selected content visible |
| Reset `03/80` | Clears stored Device Assets; stored password association/state is also cleared by the observed workflow | Full emulator-managed state reset; password runtime is not implemented |
| Device information | Tested 64×64 reports MCU `5.11` | Implemented through the 9-byte FA03 Device Info response; app-facing release bytes remain separate from the internal build number |

### Time model

After a valid app time synchronization the original unit continues Alarm execution after BLE disconnect while power remains applied. After a power cycle the previously configured Alarm/Program cannot execute until time is synchronized again. This is consistent with a volatile software clock and no persistent RTC.

### Reset policy

The emulator implements destructive clearing of emulator-managed persistent state. The live behavior of `03/80` was later refined; the current runtime also clears the volatile Preset/Default bank. Carousel, Preset, Alarm and Schedule media/metadata, stored brightness, ECO and rotation are cleared, but the already synchronized volatile software clock is preserved because the BLE session is still alive. The matrix remains logically ON and black, ready for the next app command. A real power cycle remains distinct and follows the normal boot policy. The unverified password runtime has been removed, so reset currently has no password state to clear.



## Historical password timing experiment

Historical development builds explored password SET/VERIFY acknowledgements and timing, but those runtime experiments were removed because the official app never completed the SET-password flow. Source review also showed that the nominal “deferred main-loop” timing was not implemented as independently as originally described. Those builds are historical experiments only and must not be treated as protocol evidence.

Current firmware does **not** implement password SET/VERIFY runtime behavior. The observed packet framing, decimal-pair encoding, app-side `pwdByMac` caching, and 7-byte-write / 5-byte-notification evidence remain documented as partial reverse-engineering findings.


## Preset / Default (`06/02`) - CAPTURED, IMPLEMENTED AND HARDWARE-VALIDATED

Official-app captures on the 64x64 profile show a second media bank, distinct from the persistent Device Assets Carousel. The app section is labelled **Preset / Default**.

### Activation command

The activation command is:

```text
06 02 <count> <slot0> ... <slotN>
```

The two-byte little-endian packet length precedes the command/subcommand as usual. Captured examples include:

```text
07 00 06 02 02 0E 0F
08 00 06 02 03 0E 0F 10
0A 00 06 02 05 0E 0F 10 11 12
```

The observed device-slot range is `0x0E..0x13` (14..19), giving a maximum of six items. The order after `count` is the playback order.

### Media transfer

Preset items use the ordinary 16-byte Bulk header and may be mixed media. A hardware capture of `TEXT "PJ" -> image -> TEXT "Ciao"` produced:

```text
type=3 slot=14 timeSign=5
type=1 slot=15 timeSign=5
type=3 slot=16 timeSign=5
06/02 count=3 slots=14,15,16
```

A separate five-image capture used slots 14, 15, 16, 17 and 18. Large objects use the normal multi-packet Bulk continuation model: byte 4 is observed as `0x00` on the first logical packet and `0x02` on continuation packets, while total media size, CRC, `timeSign` and slot remain stable. Intermediate accepted chunks receive transfer ACK `0x01`; the CRC-valid final chunk receives `0x03`.

For all captured Preset media, Bulk bytes 13..14 (`timeSign`, LE16) were `5`, including TEXT, small images and multi-packet images. Original-hardware visual observation is approximately **3 seconds per item**, and the app exposes no dwell setting. Therefore the emulator does **not** interpret `timeSign=5` as five seconds; it preserves the field as opaque metadata. GIF/image items use the directly observed ~3000 ms dwell. TEXT timing is content-aware and follows the renderer behavior documented below.

### Storage and lifetime

The app re-sends the selected Preset media when the Preset is invoked, unlike the persistent Device Assets Carousel workflow. Preset media is therefore kept in a separate **volatile** LittleFS bank:

```text
/pre0.gif or /pre0.txt   <- device slot 14
...
/pre5.gif or /pre5.txt   <- device slot 19
```

The files are staging/playback storage only and are deleted on reboot or emulator reset. They are not stored in Carousel NVS metadata and are never restored by the boot policy. This is an emulator policy consistent with the captured re-upload workflow; persistence semantics of every original-device power state should not be generalized beyond the direct observations.

### Implemented behavior

- slots 14..19 are intercepted before the normal live GIF/TEXT preview path;
- GIF/image and TEXT objects are CRC-validated and committed to the volatile Preset bank;
- media does not begin playback merely because upload completes;
- `06/02` validates the count/list and starts the first available item;
- playback rotates through the supplied list using content-aware timing: GIF/image items use ~3000 ms; TEXT waits for its presentation to complete as described below;
- selecting another ordinary display mode stops Preset playback;
- a new Preset upload freezes the previous Preset on its last framebuffer while replacement assets arrive, avoiding mixed old/new playback;
- reboot/reset clears all Preset files and runtime metadata.

### Preset TEXT playback timing (hardware observation)

Preset item timing is content-aware rather than a fixed per-item delay. Images/GIFs use an observed dwell of about 3 seconds. For TEXT, continuous LEFT/RIGHT scrolling advances when the final glyph has fully left the display. PIN and viewport/page-based text modes present all required text pages once and retain the final page for about 3 seconds before the next Preset item. The captured Bulk `timeSign` remains `5` and is retained as opaque metadata; it is not interpreted as seconds. Hardware logs validated both a mixed `TEXT -> GIF -> scrolling TEXT` sequence and a five-GIF sequence, including large multi-packet objects and replacement of a currently active Preset.
## Current renderer/state and audio framing notes

These are renderer/state rules only; they do not change the BLE wire format.

- LEVEL 1 uses the global LEVEL value and `packetCounter`/last-packet freshness. A new body-part combination may be selected only on a fresh non-silent LEVEL packet. No new packet or silence freezes the current pose.
- LEVEL 3 uses a 16x16 reference perimeter with one cyan pixel ON followed by two OFF pixels, moving counter-clockwise by one perimeter pixel approximately every 95 ms.
- LEVEL 5 has four eye states and four mouth states selected independently.
- FFT keeps the existing 8-logical-band renderers. The wire frame is exactly 21 bytes (`21 00 01 02 <mode> <16 bands>`), BLE ATT writes may split/coalesce frames, and adjacent wire-band pairs are averaged into the 8 logical renderer bands.
- Clock `showDate` is treated as a user preference rather than an unconditional persistent update during the approximately 1-second entry window.
- On 64x64, Clock styles 0 and 3 render time and date simultaneously when date display is enabled.
- Preset transfer indication remains active across multiple slot uploads and ends on `06/02` activation or a 5-second safety timeout.


### FA02 Audio/Rhythm stream framing

Audio/Rhythm traffic is not assembled with the normal FA02 little-endian logical-length parser. Two fixed wire-frame types are recognized before normal FA02 reassembly:

```text
LEVEL: 06 00 00 02 <level> <mode>        = 6 bytes
FFT:   21 00 01 02 <mode> <16 bands>     = 21 bytes
```

The leading bytes `21 00` in an FFT frame must **not** be decoded as a normal LE16 packet length of 33. Captures show that a 33-byte ATT write can contain one complete 21-byte FFT frame followed by the first 12 bytes of the next frame. The emulator retains those 12 bytes and completes the next frame from the following ATT write.

At a partial audio boundary, recognized normal FA02 commands take ownership immediately and discard the incomplete audio frame. This prevents stale Audio/Rhythm state from swallowing Reset, Carousel, Clock, GIF/image or other normal commands. Partial audio state is also cleared after a 1-second no-progress timeout, on BLE disconnect and on protocol reset.

### Graffiti raster implementation note

The release implementation follows the original-hardware capture above: type `0x00` is handled only by the dedicated 9-byte Graffiti raster path and is not admitted into the normal 16-byte GIF/RAW/TEXT Bulk state machine. This separation prevents a partial or malformed Graffiti transaction from taking ownership of unrelated Bulk traffic.
