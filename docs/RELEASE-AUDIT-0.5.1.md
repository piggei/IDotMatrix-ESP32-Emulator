# Release Audit - iDotMatrix ESP32 Emulator 0.5.1

**Release:** 0.5.1  
**Build:** 172  
**Audit date:** 2026-09-20

## Scope

This audit compared the Build 171 development source, the project README and internal documentation, the supplied Wiki export, the development handoff report, and the Android Bluetooth HCI snoop captured while the official application communicated with an original 64x64 iDotMatrix device.

## Release-critical findings resolved

1. **Version drift** - development material still mixed Build 169/170/171 wording and the README/build identity was not aligned to a final public release. The release is now consistently identified as `0.5.1 / Build 172`, with firmware signature `IDOTMATRIX_FW=0.5.1-B172`.
2. **Graffiti protocol ambiguity** - the original-hardware HCI trace confirms the dedicated 9-byte full-raster Graffiti path. The release keeps it separate from normal 16-byte Bulk.
3. **Generic Bulk routing** - the normal Bulk parser is restricted to types `0x01..0x03`; type `0x00` can no longer fall through into generic Bulk parsing after the dedicated Graffiti handler.
4. **Diagnostic release defaults** - `GRAFFITI_PROTOCOL_DEBUG` is disabled by default.
5. **PlatformIO default target** - the default environment is the primary qualified `matrixportal_s3_hub75_64` target rather than the isolated iOS diagnostic profile.
6. **Documentation/Wiki drift** - auto-rotation is documented as supported and hardware-qualified on MatrixPortal S3; GY-521 / MPU-6050 remains planned, not supported.
7. **Repository hygiene** - the pull-request template now references `FUTURE-WORK.md`, and Python cache/test artifacts are excluded by `.gitignore`.

## Original-hardware protocol verification

The supplied HCI snoop confirms the 64x64 Graffiti transfer as three logical packets of 4105 bytes, each carrying a 9-byte header plus 4096 RGB bytes. The first packet marker is `0x00`; continuation packets use `0x02`; total size is 12288 bytes. The original device responds `05 00 00 00 02` after the first two packets and `05 00 00 00 01` after the final packet. No CRC field or separate finalization packet was observed for this transport.

The same trace was also used to cross-check ordinary command families. Exact observed command/ACK examples are documented in `PROTOCOL.md` and `docs/captures/15-original-hardware-command-crosscheck.txt`. These include time synchronization, DIY/Graffiti mode entry/exit, Clock, Device Assets, Countdown, Preset activation, Program state and display power.

## Regression boundary

The release intentionally preserves the previously qualified 0.5.0 / Build 162 behavior for existing subsystems. The Audio/Rhythm LEVEL and FFT framing, normal RAW type `0x02`, GIF, TEXT, Carousel, Preset, Alarm and Schedule paths were not redesigned as part of the finalization.

## Static validation performed

- orientation mount-mapping regression test: PASS;
- Graffiti original-hardware raster protocol test: PASS;
- release metadata and routing assertions: PASS;
- repository scan for stale active pre-release identifiers outside historical release notes: PASS;
- protocol documentation and Wiki cross-check updated.

## Validation not performed in this audit environment

PlatformIO is not installed in the audit runtime, so a fresh firmware compile/link was not executed here. Physical hardware regressions were also not repeated by this audit environment. Before publishing binaries, perform the hardware checklist in `docs/RELEASE-VALIDATION.md`, especially normal RAW, full-raster Graffiti, Clock/TEXT/GIF under auto-rotation, Audio/Rhythm, Schedule multipart media, reboot and LittleFS persistence.

## Release decision

The source tree is prepared and internally consistent for the 0.5.1 / Build 172 release. Binary publication should follow a successful local PlatformIO build and the final hardware smoke/regression checklist.
