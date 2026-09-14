# iOS Connection Errors 10011 / 10019 — Diagnostic Investigation

## Current diagnostic build

- Release: `0.4.1-dev`
- Build: `120`
- Base stable runtime: `v0.4.0 / BUILD 118`
- Purpose: GATT/CCCD evidence gathering
- Workaround status: **none yet**

BUILD 120 intentionally does not guess at an iOS workaround. It instruments the BLE/GATT layer so the next external trace can identify where the iOS session diverges from Android.

## What BUILD 119 established

An external tester captured both iOS and Android sessions using the same ESP32 hardware.

Android:

1. establishes BLE;
2. sends the normal time-sync packet on FA02;
3. receives the FA03 acknowledgement;
4. later sends normal Clock commands on FA02;
5. receives the expected FA03 acknowledgements.

iOS:

1. establishes BLE;
2. receives the emulator's delayed Device Info push;
3. sends no FA02 application write;
4. sends no AE01 application write;
5. reports Clock configuration error `10011` and GIF send error `10019`.

This moves the investigation below the iDotMatrix command parser: the failure appears to occur during or immediately after service/characteristic discovery, subscription setup, or app-side session initialization.

The reporter also had an unreadable/unformatted LittleFS partition. That is independent of the missing iOS FA02 traffic, but BUILD 120 changes the repository default so a failed LittleFS mount is automatically formatted/retried.

## What BUILD 120 adds

Relevant lines begin with:

```text
[IOSDIAG]
```

In addition to BUILD 119 logging, BUILD 120 traces:

- FA03 characteristic reads;
- AE02 characteristic reads;
- FA03 CCCD (`0x2902`) reads and writes;
- AE02 CCCD (`0x2902`) reads and writes;
- decoded notification/indication enable bits;
- FA03/AE02 notification callback/status events;
- expanded GATT counters in periodic and disconnect summaries.

A normal notification subscription should typically produce a CCCD value equivalent to:

```text
01 00
```

for notifications enabled. BUILD 120 records the actual value rather than assuming it.

## Test requested from the iOS reporter

1. Flash `v0.4.1-dev / BUILD 120`.
2. Open Serial Monitor at the firmware baud rate.
3. Reset or power-cycle the ESP32.
4. Start capturing Serial output before opening the iDotMatrix app.
5. Open the official iDotMatrix iOS app.
6. Connect to the emulator normally.
7. Wait approximately 5 seconds after the app reports connected.
8. Select Clock and wait for the `10011` error if it still occurs.
9. Select one GIF/animation and wait for the `10019` error if it still occurs.
10. Keep the log running for another 10 seconds.
11. Attach the complete log from boot through the end of the test.

Do not crop the trace to the final error. The GATT/CCCD events immediately after connection are the most important evidence.

Please also confirm again:

- iPhone model;
- exact iOS version;
- exact iDotMatrix app version shown by iOS/App Store (`1.0.9` was previously reported; please verify whether this is actually `1.9.0`);
- ESP32 board model.

An additional Android BUILD 120 trace is useful but optional because BUILD 119 already provided a successful Android application-level reference. If convenient, capturing Android again will provide a direct CCCD/subscription comparison.

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
