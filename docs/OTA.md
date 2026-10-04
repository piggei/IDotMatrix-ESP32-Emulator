# OTA Firmware Update - Waveshare ESP32-S3 RGB Matrix

## Scope

OTA support was introduced during the 0.6.0 Waveshare bring-up and is retained in `0.6.0-dev.3 / Build 194` for the Waveshare ESP32-S3 RGB Matrix profile only. It is intentionally a maintenance service, not a permanently exposed Wi-Fi control interface.

The normal emulator runtime keeps Wi-Fi disabled. BLE, HUB75 rendering and all existing iDotMatrix behavior continue to run without a Wi-Fi connection.

## Partition model

The Waveshare profile uses `partitions/idotmatrix_waveshare_s3_32mb_ota.csv`:

| Partition | Size | Purpose |
| --- | ---: | --- |
| `nvs` | 20 KiB | NVS settings/persistence |
| `otadata` | 8 KiB | ESP32 OTA selection metadata |
| `ota_0` | 3 MiB | application slot A |
| `ota_1` | 3 MiB | application slot B |
| `spiffs` | 25.875 MiB | Arduino LittleFS media storage |
| `coredump` | 64 KiB | ESP32 crash dump |

The LittleFS partition starts after both application slots and is not rewritten by a normal firmware OTA update. This is the basis for preserving Carousel/media content across firmware updates.

## Entering OTA maintenance mode

1. Boot the firmware normally.
2. Wait until the emulator is running.
3. Press and hold the on-board **BOOT** button for approximately two seconds.
4. Release the button after the serial log reports that the OTA access point has started.

Do not hold BOOT while resetting/powering the board; GPIO0 is also a boot strap. The maintenance trigger is designed to be used after normal firmware startup.

The firmware starts an access point named:

```text
IDotMatrix-OTA-XXXXXX
```

where `XXXXXX` is derived from the ESP32 chip identifier.

Build 194 default password:

```text
idotmatrix
```

After joining the access point, Build 194 starts a captive DNS service and handles the common Android, Apple and Windows connectivity probes. On supported clients the operating system should automatically offer or open the **iDotMatrix Firmware Update** page.

Automatic captive-portal opening is initiated by the client operating system and therefore cannot be guaranteed on every device or network configuration. The direct address always remains available as a fallback:

```text
http://192.168.4.1/
```

The password can be overridden in `src/IDotMatrixUserConfig.h` with:

```cpp
#define IDOTMATRIX_OTA_AP_PASSWORD "your-password"
```

Use at least eight characters.

## Upload

Upload the `firmware.bin` produced by:

```bash
pio run -e waveshare_s3_rgbmatrix_64x64
```

The expected file is:

```text
.pio/build/waveshare_s3_rgbmatrix_64x64/firmware.bin
```

The Arduino ESP32 `Update` API writes the image to the inactive OTA application slot. The active image remains selected until the new upload has been completely written and finalized.

After a successful upload the web page reports success and the board reboots automatically.

## Failure behavior

An upload that is aborted or fails before finalization does not intentionally switch the boot target. Build 194 calls `Update.abort()` for an aborted HTTP upload and keeps the currently running firmware selected.

This is **not yet a claim of automatic post-boot rollback** for a firmware image that uploads successfully but later fails during startup. Post-boot validation/rollback is a separate hardening item for a later build.

## Build 194 field gate

Verify at least:

1. initial USB flash of Build 194;
2. normal BLE/display operation before OTA mode is entered;
3. BOOT-button hold starts the AP;
4. joining the OTA AP triggers the captive-portal page on at least one supported client;
5. the OTA page remains reachable directly at `192.168.4.1` as fallback;
6. upload of a newer test build succeeds and reboots;
7. firmware release/build changes after reboot;
8. Carousel, Alarm, Schedule and NVS-backed settings survive;
9. an intentionally interrupted upload leaves the previous firmware bootable;
10. Wi-Fi is not started after a normal reboot unless OTA maintenance mode is triggered again.

## Security model

Build 194 OTA is a physically triggered local maintenance service. It is not intended for exposure on an untrusted routed network. The AP password should be changed for deployed devices.
