# iOS Connection Errors 10011 / 10019 — Diagnostic Investigation

## Current diagnostic build

- Release: `0.4.1-dev`
- Build: `121`
- Base stable runtime: `v0.4.0 / BUILD 118`
- Purpose: captured 32x32 manufacturer/model identity A/B test
- Workaround status: **none yet**

BUILD 121 is intentionally a single-variable follow-up to BUILD 120. The GATT diagnostics and automatic Device Info push are preserved, while the emulator presents itself as a 32x32 model using a complete manufacturer record captured from real hardware.

The tester can continue using the physical 16x16 WS2812B matrix because the firmware renders a logical 32x32 framebuffer and downscales it through the existing preview path.

### BUILD 121 experimental identity

```text
logical screen type: 0x03
logical resolution:  32x32
physical preview:    16x16
manufacturer data:   54 52 00 70 03 04 0F 00 01 04
```

The manufacturer bytes above are copied exactly from a real 32x32 iDotMatrix capture supplied by another reverse-engineering project. They are used only as an experiment and are **not** asserted to be valid for 16x16 or 64x64 hardware.

## Why this test exists

BUILD 120 showed that iOS:

1. establishes the BLE connection;
2. writes `01 00` to the FA03 CCCD;
3. writes `01 00` to the AE02 CCCD;
4. receives the emulator's Device Info notification without an obvious BLE error;
5. then sends no FA02/AE01 application commands before reporting Clock error `10011` and GIF error `10019`.

Android performs the same subscriptions and then immediately begins normal FA02 traffic.

This makes device/model identification one of the strongest remaining candidates. Android may tolerate the emulator's shorter synthetic manufacturer record while the iOS app may require fields that are present on original hardware.

BUILD 121 tests that hypothesis without simultaneously removing the unsolicited Device Info push or changing BLE libraries.

## Test requested from the iOS reporter

1. Flash `v0.4.1-dev / BUILD 121`.
2. Keep the physical LED matrix wired exactly as in the previous test.
3. Open Serial Monitor before starting the app.
4. Reset/power-cycle the ESP32.
5. Open the official iDotMatrix iOS app.
6. Connect to the emulator.
7. Wait a few seconds after it appears connected.
8. Select Clock.
9. If Clock works, report that immediately and try a normal adjustment such as brightness or clock style.
10. Select one GIF/animation.
11. Keep the log running for at least another 10 seconds.
12. Disconnect Bluetooth/app normally and attach the complete log.

Please report whether the app now identifies the device differently or exposes different 32x32-specific UI/content.

The most important result is whether normal lines such as:

```text
FA02 RX ...
```

begin appearing on iOS.

### Interpretation

- **If iOS starts sending FA02 commands:** the captured 32x32 advertising/model identity materially changes app initialization, strongly implicating manufacturer/model metadata.
- **If iOS still sends no FA02 commands:** the identity hypothesis is weakened and the next controlled test should remove the emulator's unsolicited Device Info push while reverting/controlling the advertising identity.
- **If behavior changes only partially:** preserve the complete log; this may reveal which application stage is unlocked.


## LittleFS note

The repository default is now:

```cpp
#define LITTLEFS_FORMAT_ON_MOUNT_FAIL 1
```

The firmware still attempts a normal non-destructive mount first. Formatting occurs only after mount failure.

This should resolve first-use/incompatible-filesystem cases automatically. Anyone attempting forensic recovery of an unreadable filesystem should set the option back to `0` before booting.

## Next comparison

In parallel with the external iOS test, the emulator's advertising and complete GATT database should be compared against an original iDotMatrix device using nRF Connect or an equivalent BLE inspector.

The most relevant comparison points are:

- complete manufacturer data length/content;
- advertised versus scan-response services;
- FA02/FA03 properties;
- AE01/AE02 properties;
- CCCD presence and permissions;
- any additional descriptors;
- connection parameters if materially different.

Only after these captures should an emulator-side compatibility workaround be considered.
