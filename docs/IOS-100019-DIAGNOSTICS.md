# iOS Connection Errors 10011 / 10019 — Diagnostic Investigation

## Current diagnostic build

- Release: `0.4.1-dev`
- Build: `122`
- Base stable runtime: `v0.4.0 / BUILD 118`
- Purpose: unsolicited Device Info sequencing A/B test
- Workaround status: **none yet**

BUILD 122 keeps the complete BUILD 121 32x32 identity experiment in place and changes exactly one handshake behavior: the emulator no longer sends Device Info spontaneously after BLE connection.

### Preserved from BUILD 121

```text
logical screen type: 0x03
logical resolution:  32x32
physical preview:    16x16
manufacturer data:   54 52 00 70 03 04 0F 00 01 04
```

The full manufacturer record above was captured from a real 32x32 iDotMatrix unit. BUILD 121 confirmed that the iOS app recognizes this identity correctly as a 32x32 device, but still sends no normal FA02 application traffic during the active session.

## What BUILD 122 changes

Previous diagnostic builds scheduled an automatic Device Info notification about 1.2 seconds after connection. BUILD 122 suppresses that unsolicited notification.

Conceptually:

```text
CONNECT
  -> FA03 CCCD subscription
  -> AE02 CCCD subscription
  -> no spontaneous Device Info notification
  -> wait for iOS app-driven traffic
```

If the app explicitly requests Device Info using the normal protocol, the emulator still responds normally. Only the unsolicited post-connect push is disabled.

This tests whether the iOS app uses a stricter handshake/state machine than Android and may reject or stall when Device Info arrives before it expects it.

## Test requested from the iOS reporter

1. Flash `v0.4.1-dev / BUILD 122`.
2. Keep the same physical 16x16 matrix and the same local GPIO adjustment used for BUILD 121.
3. Do **not** change `IDOTMATRIX_SCREEN_TYPE`, `ENABLE_LOGICAL_TO_PHYSICAL_PREVIEW`, or the captured manufacturer identity.
4. Open Serial Monitor before opening the iDotMatrix app.
5. Reset/power-cycle the ESP32.
6. Open the official iDotMatrix iOS app and connect.
7. Wait about 5 seconds after it appears connected.
8. Open Clock and try one Clock setting.
9. Try one GIF/animation.
10. Leave the log running for another 10 seconds.
11. Disconnect normally and attach the complete Serial output.

Please also report whether **Device Information** in the iOS app still shows the 32x32 model and whether MCU information is present or missing before any command is sent.

The most important result remains whether any normal line such as:

```text
FA02 RX ...
```

appears during the active connection.

### Interpretation

- **FA02 traffic begins:** the unsolicited Device Info notification was materially affecting iOS session initialization.
- **Still no FA02 traffic:** the sequencing hypothesis is weakened; the next step should compare startup traffic from real hardware, especially any AE02 notification or other app-expected event, before changing BLE libraries.
- **Partial change:** preserve the complete trace; even a different error timing or Device Information behavior is useful evidence.


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
