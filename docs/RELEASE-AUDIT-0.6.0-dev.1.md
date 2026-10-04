# 0.6.0-dev.1 / Build 191 Development Audit

**Release:** `0.6.0-dev.1`  
**Build:** `191`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.1-B191`

## Scope

Build 191 starts the 0.6.0 development line from the stable 0.5.2 / Build 190 source. The runtime delta is intentionally limited to:

- a new Waveshare ESP32-S3 RGB Matrix target;
- target-specific HUB75 pin mapping;
- 32 MB flash / 16 MB PSRAM configuration;
- dual-slot OTA partitioning;
- a physically triggered Wi-Fi/HTTP OTA maintenance service;
- associated diagnostics, tests and documentation.

No GIF PSRAM staging/cache/prefetch, MicroSD, PCF85063, QMI8658, SHTC3 or audio integration is enabled in this build.

## Static verification

The dependency-free repository regression runner passes **41 tests** in the packaging environment.

Coverage added for Build 191 includes:

- Waveshare environment selection and memory flags;
- exact HUB75 GPIO mapping;
- explicit absence of deferred peripheral backends from the Build 191 profile;
- dual 6 MiB OTA application slots and exact 32 MB partition-boundary check;
- OTA `Update` write/finalize/abort path;
- OTA HTTP servicing outside the emulator runtime-state mutex;
- host C++ syntax compilation of `IDotMatrixOta.cpp` against minimal Arduino/Wi-Fi/WebServer/Update stubs;
- release/build identity;
- update-helper Waveshare development defaults.

The existing 0.5.2 protocol, RTC, buzzer, orientation, TEXT, Graffiti, boot-policy and timebase regression coverage remains passing.

## Partition audit

`partitions/idotmatrix_waveshare_s3_32mb_ota.csv` is contiguous and ends exactly at `0x02000000` (32 MiB):

- NVS: `0x9000 + 0x5000`;
- OTA metadata: `0xE000 + 0x2000`;
- `ota_0`: `0x10000 + 0x600000`;
- `ota_1`: `0x610000 + 0x600000`;
- LittleFS (`spiffs` subtype): `0xC10000 + 0x13F0000`.

The PlatformIO profile caps the application image at 6 MiB to match each OTA slot.

## OTA safety model

The Build 191 OTA service is disabled on existing profiles and enabled by default only on the Waveshare target. It does not start Wi-Fi during normal operation. Holding BOOT/GPIO0 for approximately two seconds while the firmware is already running starts the local AP and HTTP updater.

The inactive application slot is written with Arduino-ESP32 `Update`. A completed upload must successfully finalize before the firmware schedules a reboot. Aborted uploads call `Update.abort()` and do not intentionally select the incomplete image.

Automatic rollback from a successfully uploaded image that later fails during startup is **not claimed** in Build 191.

## Documentation/package checks

- source relative Markdown links: PASS;
- Wiki page/image links: PASS;
- update helper `bash -n`: PASS;
- hardware/demo asset catalog preserved from the stable baseline;
- no local `src/IDotMatrixUserConfig.h` intended for packaging;
- no PlatformIO build output, firmware binary or Python cache intended for packaging.

## Toolchain limitation

PlatformIO / pioarduino and Arduino CLI are not installed in the packaging environment. Therefore this audit does **not** claim a complete ESP32 firmware compile of the new Waveshare environment. The physical qualification gate begins with compiling and flashing `waveshare_s3_rgbmatrix_64x64` on the user's development machine.

## Required physical gate

See `WAVESHARE-B191-QUALIFICATION.md`. Build 191 remains a development build until the Waveshare board passes native HUB75/BLE operation, successful OTA, persistence after OTA and interrupted-upload recovery.
