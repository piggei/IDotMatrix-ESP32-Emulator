# Release Audit - iDotMatrix ESP32 Emulator 0.6.0-rc.1

## Purpose

This audit covers the first 0.6.0 release candidate. The RC packages the hardware-qualified runtime as a publication candidate and removes internal development chronology from user-facing documentation.

## Public identity

- Release candidate: `0.6.0-rc.1`
- Firmware signature: `IDOTMATRIX_FW=0.6.0-rc.1`
- Default PlatformIO environment: `waveshare_s3_rgbmatrix_64x64`

Internal numeric revision identifiers may still exist in source-level diagnostics and regression-test filenames for engineering traceability; they are not part of the public release documentation or compatibility contract.

## Hardware evidence

The release candidate carries the following physically verified results:

- Waveshare logical 16x16/32x32/64x64 scaling on a physical 64x64 HUB75 panel: PASS.
- Static image, GIF and TEXT operation: PASS.
- Mixed PNG/GIF/TEXT Carousel lifecycle: PASS.
- Repeated Carousel replacement with hardened NimBLE host stack: PASS.
- Transactional recovery after interrupted Carousel replacement: PASS.
- Maintenance OTA and interrupted-upload recovery: PASS.
- BOOT short press software reboot: PASS.
- Waveshare ES8311/I2S1 speaker initialization: PASS.
- Synthesized BLE connection beep: PASS.
- Synthesized Countdown completion notification: PASS.
- Synthesized Program/Schedule notification: PASS.
- Synthesized repeating Alarm notification: PASS.
- 100% codec-volume default on the tested Waveshare speaker path: PASS.
- Notification backend requires no stored WAV/PCM assets: PASS by design.

## Documentation/package cleanup

- Public documentation is organized around current behavior, hardware support, protocol evidence, configuration, validation and future work.
- Internal development-build chronology has been removed from the release history and current release documents.
- Historical per-release audit/note files that duplicated `HISTORY.md` have been removed from the RC package.
- Temporary codec bring-up dumps are not part of normal release diagnostics.
- Generated firmware, `.pio`, Python caches and local user hardware configuration are excluded from the source package.

## Remaining exclusions

Password completion/enforcement, complete iOS/RCSP compatibility, automatic post-boot OTA rollback, the supplied GY-521 qualification, MatrixPortal plus external ICM-20689 qualification, and Waveshare PCF85063/QMI8658/SHTC3/MicroSD support remain outside the RC scope.

## RC decision

The source tree is suitable for RC validation. Publication as final `v0.6.0` should follow successful compilation of the intended targets and a final physical smoke test using the checklist in `RELEASE-VALIDATION.md`.
