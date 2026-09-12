# iDotMatrix Protocol Comparison

This document cross-checks the protocol independently derived by this project against other public iDotMatrix implementations and reports from original hardware. It is intended as a research aid, not as a claim that every iDotMatrix model uses an identical protocol.

## Roles

| Project | BLE role | Main purpose | Hardware evidence |
|---|---|---|---|
| This project | **Peripheral / server emulator** | Emulates an iDotMatrix device for the official app | 16x16 ESP32 emulator plus direct comparison with an original 64x64 device |
| derkalle4/python3-idotmatrix-client | Client / central | Controls original displays | 16x16 and 32x32 community use |
| 8none1/idotmatrix | Client / research | Controls and reverse-engineers original displays | Original display captures documented |
| dallanwagz/idotmatrix-ha | Client / central | Home Assistant control of original display | 32x32 hardware validated |
| markusressel/idotmatrix-api-client | Client / central | Python API client | Multi-model client implementation |

## Command comparison

Legend: **Confirmed** = independently corroborated; **Local** = currently strongest evidence is our app/emulator work; **Partial** = command is understood but some device-side behavior remains unresolved.

| Feature | ESP32 emulator | Other public work | Confidence / notes |
|---|---:|---:|---|
| BLE peripheral emulation | Yes | No equivalent found | **Local / distinctive feature** |
| Official app connects directly | Yes | Client projects use original hardware | **Confirmed locally** |
| Device information | Yes | Present in client implementations | **Confirmed** |
| Time synchronization | Yes | derkalle4 and other clients | **Confirmed** |
| Screen power | Yes | derkalle4, hardware clients | **Confirmed** |
| Brightness | Yes | Multiple clients | **Confirmed** |
| 180-degree flip | Yes | derkalle4 / hardware clients | **Confirmed** |
| Solid RGB | Yes | Multiple clients | **Confirmed** |
| Graffiti / DIY | Yes | Multiple clients | **Confirmed** |
| Clock | Yes | derkalle4 and hardware clients | **Confirmed; style details may vary by model** |
| Scoreboard | Yes | derkalle4 and hardware clients | **Confirmed** |
| Countdown | Yes | derkalle4: modes 0 disable, 1 start, 2 pause, 3 restart | **Command confirmed; exact app/device state behavior still partial** |
| Stopwatch / chronograph | Yes | derkalle4: 0 reset, 1 start, 2 pause, 3 continue | **Command confirmed; exact app/device state behavior still partial** |
| Text transfer | Yes | derkalle4 and 8none1 | **Strongly corroborated** |
| GIF transfer | Yes | Multiple clients / captures | **Strongly corroborated** |
| Intermediate ACK 0x01 | Yes | Observed by 8none1 during transfers | **Independently corroborated** |
| Completion ACK 0x03 | Yes | Observed by 8none1 at transfer completion | **Independently corroborated** |
| Alarm | Yes | Limited public support found | **Mostly local** |
| Programs / Schedule | Yes | No comparable public implementation found during review | **Local / important finding** |
| Music LEVEL modes | Yes | No comparable implementation found during review | **Local** |
| Music FFT modes | Yes | No comparable implementation found during review | **Local** |
| Scheduled PNG/GIF/Text | Yes | No comparable public implementation found during review | **Local** |
| Device Assets carousel | Hardware-tested in BUILD 95; upload blackout added and hardware-tested in BUILD 96; consolidated in BUILD 97 | Hardware-validated public RE documents one 12-slot device bank, `timeSign`, `imageIndex`, material wipe/setup and `0A/01`; local app captures corroborate 12-slot pushes and dwell/index fields | **Strong cross-corroboration; core emulator sequencing hardware-tested** |

## Device Assets carousel cross-check

Cross-source agreement is strong for a single 12-slot device bank, `imageIndex` 0..11, per-slot `timeSign`, material setup (`02/01`) and Assets view (`0A/01`). Public original-hardware reverse engineering reports autonomous GIF carousel playback and persistent slot storage.

Local official-app captures add two sequencing/content observations that matter to the emulator implementation:

- `0A/01` can be observed before a later page push and is not necessarily repeated after that push. BUILD 94 incorrectly treated it as a mandatory post-upload transaction terminator.
- A full 12-position push contains a `DataType.TEXT` Bulk between GIF indices 4 and 6. BUILD 94 parsed it as live TEXT, which cleared the carousel upload context and caused subsequent GIF indices 6..11 to be handled as live GIFs. BUILD 95 stores carousel-range TEXT as a slot and preserves the replacement transaction; mixed GIF/TEXT playback was subsequently hardware-tested successfully.

The public RE currently documents persistent carousel slots as GIF-only. Mixed TEXT slot persistence/playback is therefore a **project-observed and emulator-hardware-tested extension**. Equivalent TEXT persistence/playback on original iDotMatrix hardware remains unverified and is not generalized into a universal protocol claim. BUILD 95 uses a 3-second upload-idle settle only because no explicit post-push frame has been observed in the local short-page captures; that timer is emulator policy rather than protocol evidence. BUILD 96 additionally forces the physical matrix black during replacement as an emulator UX policy, without altering `screenOn`; this blackout was hardware-tested successfully and the same behavior is retained in BUILD 97.

## ACK semantics

One of the strongest cross-project confirmations concerns transfer ACK status values. During our Schedule reverse engineering, returning status `0x01` after a complete activity caused the official app to stop and report an error. Returning `0x03` allowed the app to continue normally. Independent original-hardware research by 8none1 documents the same distinction during media transfer:

- `0x01` — accepted / transaction still in progress / more data expected
- `0x03` — transaction complete

This should be treated as a general protocol convention unless a command-specific exception is observed.

## Matrix sizes and model differences

Public clients support or discuss 16x16, 32x32 and 64x64 displays, but this does **not** imply identical payload limits or rendering behavior. Known or suspected model-dependent areas include:

- device-info dimensions;
- image/GIF dimensions;
- framebuffer and coordinate ranges;
- BLE transfer/chunk limits;
- firmware-specific features;
- clock/text rendering behavior.

The emulator now defaults to native 16x16 (`0x01`) while also supporting tested app profiles `0x03` (32x32) and `0x04` (64x64). The larger profiles were exercised against the official app using an optional 64/32-to-16 diagnostic preview; that preview is disabled by default. 64x64 cloud GIF transfer and playback were verified on the classic ESP32 using LittleFS-backed media handling.

## Research policy

Raw captures take precedence over interpretation. When an external implementation disagrees with our current interpretation, the discrepancy should be documented and tested rather than silently reconciled. Model and firmware version should be recorded whenever possible.

## Related projects

- https://github.com/derkalle4/python3-idotmatrix-client
- https://github.com/8none1/idotmatrix
- https://github.com/dallanwagz/idotmatrix-ha
- https://github.com/markusressel/idotmatrix-api-client
- https://github.com/nj-designs/go-idot

## Direct original-hardware observations (64×64)

A physical 64×64 iDotMatrix is now available as a protocol oracle. Direct observations take precedence over third-party inference for the tested behavior. Confirmed: persistent Device Assets boot resume; no persistent RTC; volatile timekeeping after app synchronization; Alarm works after BLE disconnect but not after reboot without re-sync; repeating Alarm trill; roughly 30-second repeating Program trill; silent Countdown completion; Power Saving brightness reduction; 180-degree Flip; reset removes Device Assets and the stored password association; Device Information shows MCU `5.11`. BUILD 108 established that advertising/manufacturer-data changes alone do not control the app-displayed MCU value; BUILD 109 confirmed the 9-byte Device Info response as the controlling path, with emulator bytes `00 04` displayed as MCU `0.04`.

The emulator intentionally differs in several UX choices: no boot animation, a single connection beep instead of the original connection logo/animation, persistent Cloud/Graffiti display, one-shot Program trill, one-shot Countdown completion trill, and custom Snowflake/Laser text effects.


## Password capture notes

Official-app Android logcat shows the app restoring a cached password (`pwdByMac`) for the connected device. Two user password submissions correspond to 7-byte GATT writes followed by 5-byte FA03 notifications, matching the strongly indicated `05/02` VERIFY transaction shape. Direct app captures also establish the `04/02` SET frame and decimal-pair encoding.

BUILD 101-103 attempted emulator SET/VERIFY support and different ACK timing strategies, but the official app remained on the Set Password screen. BUILD 104 removes the unverified runtime implementation. Password protocol findings remain documented, while complete SET completion semantics and enforcement behavior remain open.
