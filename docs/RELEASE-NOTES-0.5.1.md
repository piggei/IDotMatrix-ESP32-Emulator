# iDotMatrix ESP32 Emulator 0.5.1

**Build:** 172  
**Firmware signature:** `IDOTMATRIX_FW=0.5.1-B172`

Release 0.5.1 promotes the hardware-qualified orientation work and the original-hardware Graffiti protocol reconstruction into a stable public build while preserving the 0.5.0 / Build 162 behavior as the regression baseline for existing features.

## Highlights

- hardware-qualified LIS3DH automatic orientation on Adafruit MatrixPortal ESP32-S3;
- generic `IDOTMATRIX_ACCEL_MOUNT_ROTATION=0|90|180|270` compensation for custom sensor mounting;
- original-hardware-confirmed 64x64 Graffiti full-raster transport with a dedicated 9-byte header;
- three 4096-byte RGB chunks for a 12288-byte 64x64 frame, with ACK `0x02` while incomplete and `0x01` on completion;
- strict separation between Graffiti type `0x00` and the normal 16-byte GIF/RAW/TEXT Bulk types `0x01..0x03`;
- additional original-hardware HCI cross-checks for ordinary commands including time sync, DIY mode, Clock, Device Assets, Countdown, Preset activation, Program state and display power;
- release defaults cleaned: protocol tracing disabled and MatrixPortal S3 / 64x64 HUB75 selected as the default PlatformIO environment;
- README, protocol documentation and Wiki synchronized with the release state.

## Compatibility boundary

The release does not add the planned MPU-6050 / GY-521 orientation backend. Password SET/VERIFY remains partially reverse-engineered and intentionally not implemented. The dedicated Audio/Rhythm LEVEL and FFT framing and the qualified normal RAW `type=0x02` path are unchanged.

## Validation

Static protocol/orientation regression tests are included in `tests/`. Hardware qualification remains centered on MatrixPortal ESP32-S3 + 64x64 HUB75, with the existing ESP32-C3 + 16x16 WS2812 baseline retained. See `docs/RELEASE-VALIDATION.md` and `PROTOCOL.md` for the evidence and scope.
