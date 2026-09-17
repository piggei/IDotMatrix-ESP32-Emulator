# iOS development branch

## Current experiment: BUILD 144

BUILD 144 keeps the successful BUILD 142 identity/timing experiment (the official iOS app identifies the emulator as 32x32 after the delayed FA03 Device Info notification) and the BUILD 143 passive RCSP diagnostics. BUILD 143 showed that, even after FA03 and AE02 notification subscriptions are enabled, the app sends no FA02 command, no AE01 write, no characteristic read, no raw JieLi authentication packet, and no `FE DC BA ... EF` RCSP frame.

BUILD 144 adds one deliberately isolated stimulus. At 900 ms after both notification CCCDs are active, the emulator sends exactly one deterministic raw JieLi-style packet on AE02: `0x00` followed by 16 challenge bytes. This packet is **diagnostic only**. It is not documented as stock iDotMatrix behavior, it does not prove authentication, and the emulator does not forge the later `pass` stage. The test asks only whether the iOS JieLi layer reacts at all.

A useful result is any AE01 packet after the stimulus, especially `0x01` followed by 16 bytes. If AE01 remains completely silent through disconnect, the remaining gate is likely earlier than the RCSP authentication exchange and an original iPhone-to-iDotMatrix capture becomes the preferred next step.

For Thiago's test, capture the complete serial log from boot through disconnect and leave the official app connected for at least 30 seconds. No manual LightBlue writes are needed.

# Historical baseline: BUILD 142

**Release:** `v0.5.0-dev`  
**Build:** `142`  
**Branch:** `ios-dev`  
**Baseline:** BUILD 141 consolidation runtime

BUILD 142 exists to test one narrow iOS session-start hypothesis without changing the validated main-branch runtime.

## BUILD 122 baseline

Thiago's BUILD 122 capture established the following:

- classic ESP32, physical 16x16 WS2812 on GPIO17, logical 32x32 buffers;
- real 32x32 manufacturer payload `54 52 00 70 03 04 0F 00 01 04`;
- iOS connects successfully;
- FA03 notifications are enabled;
- AE02 notifications are enabled;
- the connection remains open until the app/Bluetooth is closed;
- with unsolicited Device Info suppressed, the complete session contains no FA02 writes and no AE01 writes;
- the iOS app displayed the emulator as 16x16 in that run;
- LightBlue can write the Android Clock command directly to FA02 and the emulator displays the Clock correctly.

This makes basic GATT connectivity and the FA02 Clock path unlikely to be the immediate blocker.

## BUILD 142 hypothesis

The app may require Device Info only after its notification subscriptions are ready. BUILD 142 therefore performs this sequence:

```text
BLE connect
    |
    +-- wait for FA03 CCCD notifications ON
    |
    +-- wait for AE02 CCCD notifications ON
    |
    +-- both ON -> schedule 250 ms delay
    |
    +-- send ONE FA03 Device Info notification
    |
    +-- observe FA02 / AE01 traffic
```

No speculative AE/RCSP reply is generated in this build.

## Identity used by the experiment

Advertising manufacturer payload (captured from a real 32x32 unit):

```text
54 52 00 70 03 04 0F 00 01 04
```

Experimental delayed Device Info response:

```text
09 00 01 80 04 0E 01 03 00
```

Important: the complete 32x32 Device Info packet above is **not** a direct real-32x32 capture. `04 0E` comes from the documented original 16x16 Device Info capture; BUILD 142 changes only the final profile byte to `0x03`. The test is designed to determine whether timing/profile notification is the missing iOS gate, not to declare these bytes authoritative.

## Hardware / PlatformIO target

Use:

```text
ios_dev_esp32_ws2812_32
```

The target selects:

```text
classic ESP32
physical 16x16 WS2812
GPIO17
logical 32x32 iDotMatrix profile
LittleFS via min_spiffs.csv
```

Commands:

```text
pio run -e ios_dev_esp32_ws2812_32
pio run -e ios_dev_esp32_ws2812_32 -t upload
pio device monitor -e ios_dev_esp32_ws2812_32 -b 115200
```

## Expected diagnostic sequence

A useful iOS run should contain lines similar to:

```text
[IOSDIAG] ===== BLE CONNECT =====
[IOSDIAG +...] FA03 CCCD WRITE ... notifications=ON
[IOSDIAG +...] AE02 CCCD WRITE ... notifications=ON
[IOSDIAG +...] DEVICE INFO scheduled in 250 ms after both CCCDs became active
[IOSDIAG +...] both CCCDs still active; sending one Device Info notification
[IOSDIAG +...] DEVICE INFO TX raw=09 00 01 80 04 0E 01 03 00
```

The decisive evidence is what follows. If the official app starts its application session, BUILD 142 prints:

```text
[IOSDIAG +...] FA02 RX #1 len=... head=...
```

or:

```text
[IOSDIAG +...] AE01 RX #1 len=... head=...
```

If the app remains silent, keep the complete log from boot through disconnect; the disconnect summary records FA02/AE01 counts and whether Device Info was actually sent.

## Interpretation

- **Device becomes 32x32 and FA02/AE01 traffic starts:** the delayed Device Info is a strong candidate for the missing iOS session gate.
- **Device becomes 32x32 but FA02/AE01 remains zero:** classification and application startup are separate problems; capture the original device's post-subscription GATT behavior next.
- **Device remains 16x16 and FA02/AE01 remains zero:** Device Info timing alone is insufficient; avoid inventing AE/RCSP packets until original-device evidence is available.

BUILD 141 remains the main-branch consolidation baseline until this experiment is validated.
