# iOS Connection Error 100019 — Diagnostic Build Instructions

## Build

- Release: `0.4.1-dev`
- Build: `119`
- Purpose: diagnostic tracing only
- Base runtime: `v0.4.0 / BUILD 118`

This build is intended to investigate reports that the official iDotMatrix **iOS** application fails to connect to the emulator/Usermod-compatible BLE device with application error **100019**, while the Android application works.

No protocol workaround is intentionally included in BUILD 119. The purpose is to capture the failing handshake before changing behavior.

## What is logged

Lines relevant to the investigation begin with:

```text
[IOSDIAG]
```

The firmware logs:

- BLE/GATT configuration at boot;
- configured device name;
- FA and AE service UUIDs;
- characteristic property configuration;
- advertising/manufacturer bytes;
- BLE connection event;
- every FA02 write received from the app;
- every AE01 write received from the app;
- every FA03 notification transmitted by the emulator;
- relative time in milliseconds from the connection event;
- periodic connection counters and free heap;
- BLE disconnect event and session counters.

Payload dumps are capped at 96 bytes per individual line so large media transfers do not make the initial-handshake log unusable.

## Test requested from the iOS reporter

1. Flash BUILD 119 onto the ESP32 setup that is known to work with the Android iDotMatrix app.
2. Open Serial Monitor at the same baud rate used by the firmware.
3. Reset/power-cycle the ESP32.
4. Start capturing the Serial output **before opening the iDotMatrix app**.
5. Open the official iDotMatrix iOS app.
6. Attempt to connect normally.
7. Wait until error `100019` is displayed.
8. Leave the log running for approximately 10 additional seconds.
9. Copy the complete Serial output from boot through the failed connection/disconnection.
10. Also report:
   - iPhone model;
   - iOS version;
   - iDotMatrix app version;
   - ESP32 board model;
   - whether the same ESP32/firmware setup connects successfully from Android.

Please do not crop the log to only the final error. The messages immediately before the failure are the most important part.

## Optional Android reference log

If possible, after the iOS test:

1. reboot the same ESP32;
2. capture a second complete Serial log;
3. connect using the Android iDotMatrix app;
4. leave it connected until the main application screen is usable;
5. send that complete log as well.

A failing iOS trace plus a successful Android trace from the same hardware/build provides the best comparison.

## Expected analysis

The first comparison will determine whether iOS fails:

- before any GATT write;
- before notification traffic;
- during Device Info exchange;
- during another initial FA02 command;
- after a valid protocol response;
- or after a timeout/disconnect without application data.

This should substantially narrow whether error `100019` is caused by advertising/GATT expectations, handshake ordering, response contents, or app-specific timing.
