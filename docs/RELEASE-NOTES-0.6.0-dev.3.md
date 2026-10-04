# iDotMatrix ESP32 Emulator 0.6.0-dev.3

**Release:** `0.6.0-dev.3`  
**Build:** `194`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B194`

## Purpose

Build 194 is an OTA usability hardening build on top of the Build 193 Waveshare baseline. The firmware upload path, dual-slot partition geometry and physical BOOT/GPIO0 trigger remain unchanged.

The change adds captive-portal discovery to the temporary OTA access point so supported operating systems can open the firmware upload page automatically after joining `IDotMatrix-OTA-XXXXXX`, without requiring the user to read `192.168.4.1` from the serial log.

## Changes

Build 194:

- keeps the Build 193 board, flash, PSRAM, HUB75, NimBLE and OTA upload implementation unchanged;
- adds the core ESP32 `DNSServer` captive DNS service while OTA mode is active;
- resolves arbitrary DNS names to the SoftAP address;
- redirects common Android, Apple and Windows captive-connectivity probes to the OTA root page;
- redirects unknown HTTP paths to the OTA page instead of returning `404`;
- preserves `/health` as a direct diagnostic endpoint;
- keeps `http://192.168.4.1/` as a manual fallback;
- keeps Wi-Fi disabled during normal emulator operation;
- adds regression coverage for captive DNS, probe redirects and fallback behavior.

No GIF PSRAM staging, GIF cache, Carousel prefetch, MicroSD, PCF85063, QMI8658, SHTC3 or audio support is added in this build.

## Validation

The dependency-free repository regression suite passes all 46 checked tests from the Build 194 source tree. Static regression coverage verifies the existing Waveshare NimBLE selection and additionally guards wildcard captive DNS, common OS probe redirects, unknown-path fallback and the unchanged OTA upload transaction.

A full PlatformIO build and physical Waveshare test remain required. The first field gate is:

```bash
pio run -e waveshare_s3_rgbmatrix_64x64
```

Only after a clean compile should Build 194 be uploaded to hardware.
