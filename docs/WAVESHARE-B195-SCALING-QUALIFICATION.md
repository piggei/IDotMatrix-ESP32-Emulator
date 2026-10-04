# Waveshare ESP32-S3 RGB Matrix - Build 195 Scaling Qualification

**Release:** `0.6.0-dev.3`  
**Build:** `195`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B195`

## Purpose

Build 195 is a qualification build for the existing standalone logical-to-physical display scaler on the Waveshare ESP32-S3 RGB Matrix. It does not introduce a new scaling algorithm. The B194 board, HUB75, NimBLE, OTA, partition and captive-portal baseline is retained.

The physical panel remains 64x64 for every profile. Three PlatformIO environments expose the three supported logical iDotMatrix screen types:

| Environment | Screen type | Logical framebuffer | Physical panel | Expected upscale |
| --- | ---: | ---: | ---: | ---: |
| `waveshare_s3_rgbmatrix_16x16` | `0x01` | 16x16 | 64x64 | 4x nearest-neighbour |
| `waveshare_s3_rgbmatrix_32x32` | `0x03` | 32x32 | 64x64 | 2x nearest-neighbour |
| `waveshare_s3_rgbmatrix_64x64` | `0x04` | 64x64 | 64x64 | 1:1 |

## Inherited B194 field evidence

The physical Waveshare target has already field-confirmed the OTA upload path. An intentionally interrupted OTA upload returns to the previous valid firmware, and persistent memory/state survives the interrupted-update test. Build 195 does not change that upload transaction or flash geometry.

Automatic captive-portal opening is client-controlled and is not inferred from those upload results unless separately observed.

## Static/runtime design

`MATRIX_WIDTH` and `MATRIX_HEIGHT` are selected from `IDOTMATRIX_SCREEN_TYPE`. The final HUB75 refresh always iterates the 64x64 physical panel and obtains each output pixel through `logicalToPhysicalPixel()`.

For 16x16 and 32x32 logical profiles, nearest-neighbour mapping uses:

```text
sourceX = physicalX * logicalWidth  / 64
sourceY = physicalY * logicalHeight / 64
```

Therefore:

- 16x16 -> 64x64: each source pixel is repeated exactly 4x4;
- 32x32 -> 64x64: each source pixel is repeated exactly 2x2;
- 64x64 -> 64x64: exact 1:1 copy.

The BLE manufacturer data continues to expose the selected `IDOTMATRIX_SCREEN_TYPE`, so the official app sees the logical device class associated with the firmware profile rather than the physical 64x64 panel size.

## Qualification assets

Use the checked-in visual probes under:

```text
docs/qualification-assets/waveshare-scaling/
```

Files:

```text
waveshare-scaling-16x16.png
waveshare-scaling-32x32.png
waveshare-scaling-64x64.png
```

Each image has:

- a one-logical-pixel white border;
- red top-left marker;
- green top-right marker;
- blue bottom-left marker;
- yellow bottom-right marker;
- magenta horizontal center line;
- cyan vertical center line;
- dim one-pixel checkerboard background.

This makes mirror, rotation, off-by-one errors and the replication factor immediately visible.

## Build commands

Build all three profiles from a clean tree:

```bash
pio run -e waveshare_s3_rgbmatrix_16x16
pio run -e waveshare_s3_rgbmatrix_32x32
pio run -e waveshare_s3_rgbmatrix_64x64
```

Each image must remain below the 3 MiB OTA-slot limit.

The generated firmware paths are:

```text
.pio/build/waveshare_s3_rgbmatrix_16x16/firmware.bin
.pio/build/waveshare_s3_rgbmatrix_32x32/firmware.bin
.pio/build/waveshare_s3_rgbmatrix_64x64/firmware.bin
```

The profiles can be moved between through the already-qualified local OTA upload path.

## Serial identity check

After each boot verify:

```text
IDOTMATRIX_FW=0.6.0-dev.3-B195
DISPLAY SCALE: screenType=<1|3|4> logical=<16x16|32x32|64x64> physical=64x64
```

The physical dimensions must never change between profiles.

## Physical field matrix

Run the following for each logical profile.

### 1. Boot / geometry

- stable boot and USB serial;
- HUB75 initialization checkpoints appear once;
- no blank panel, reset loop or row scrambling;
- serial reports the expected `screenType`, logical dimensions and physical `64x64` dimensions.

### 2. Qualification PNG

Display the matching qualification asset through the normal app media path.

Verify:

- all four coloured corner markers are in the expected corners;
- the full white border is visible and exactly aligned with the panel edge after scaling;
- center lines meet in the expected center;
- no mirror or 180-degree inversion is introduced by changing logical profile;
- 16x16 pixels expand to 4x4 blocks;
- 32x32 pixels expand to 2x2 blocks;
- 64x64 pixels remain 1x1.

### 3. Representative protocol modes

Repeat at least:

- Clock;
- TEXT;
- static image;
- GIF;
- Graffiti;
- Carousel.

The goal is not to compare artwork between logical classes pixel-for-pixel; it is to confirm that each mode renders through the selected logical framebuffer and reaches the full 64x64 physical panel without clipping, stale regions or coordinate errors.

### 4. BLE/app identity

For each profile verify that the official app connects normally and behaves as the corresponding 16x16, 32x32 or 64x64 iDotMatrix device class. Representative content transfer must complete successfully.

### 5. OTA profile transitions

At least one transition should be performed through OTA, for example:

```text
64x64 -> 32x32 -> 16x16 -> 64x64
```

Verify after every reboot that the newly selected logical profile is reported and persistent storage remains mounted.

## Exit gate

Build 195 is qualified when all three logical profiles pass the physical geometry test and representative Clock/TEXT/image/GIF/Graffiti/Carousel smoke tests on the 64x64 Waveshare panel.

No PSRAM placement, GIF staging/cache, media prefetch or deferred Waveshare peripheral support belongs in Build 195. Those changes begin only after the scaling gate is closed.


## Physical field result

Physical testing after packaging confirmed correct operation of all three Waveshare logical profiles on the 64x64 panel:

- logical 64x64 -> physical 64x64: **PASS**;
- logical 32x32 -> physical 64x64: **PASS**;
- logical 16x16 -> physical 64x64: **PASS**.

No scaling failure was reported in the three-profile field test. The B195 scaling gate is therefore closed and B196 may proceed as a measurement-only memory baseline without modifying the scaler.
