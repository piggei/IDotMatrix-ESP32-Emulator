# 0.6.0-dev.3 / Build 194 Development Audit

**Release:** `0.6.0-dev.3`  
**Build:** `194`  
**Firmware signature:** `IDOTMATRIX_FW=0.6.0-dev.3-B194`

## Scope

Build 194 is an OTA usability hardening build on the Build 193 Waveshare baseline. It adds captive-portal discovery only; the firmware upload transaction, flash geometry and normal runtime behavior are intentionally unchanged.

## Inherited Build 193 baseline

Build 193 resolved the Build 192 Waveshare compile mismatch by selecting the same BLE library family already used by the iDotMatrix WLED Usermod: `h2zero/NimBLE-Arduino @ 2.5.1`. Build 194 does not change that BLE backend.

## BLE compatibility review

The Waveshare-only implementation was updated for NimBLE-Arduino 2.x API requirements:

- `NimBLEServerCallbacks::onConnect(NimBLEServer*, NimBLEConnInfo&)`;
- `NimBLEServerCallbacks::onDisconnect(NimBLEServer*, NimBLEConnInfo&, int)`;
- `NimBLECharacteristicCallbacks::onWrite(NimBLECharacteristic*, NimBLEConnInfo&)`;
- `NIMBLE_PROPERTY::*` characteristic property flags;
- copied `std::string` characteristic values before parsing;
- `NimBLEServer::start()` for the GATT database;
- explicit scan-response enablement for NimBLE 2.x;
- manual advertising restart remains deferred to the main loop.

The existing legacy BLE source path remains compiled for profiles that do not define `IDOTMATRIX_USE_NIMBLE`.

## Regression status

The checked-in dependency-free test runner passes **46 tests**. Guards verify:

- exact Build 194 release/build identity;
- pinned NimBLE-Arduino 2.5.1 dependency and Waveshare-only backend selection inherited from Build 193;
- retention of the previous legacy BLE path for existing profiles;
- unchanged Waveshare WLED-qualified framework and partition geometry;
- `DNSServer` wildcard DNS startup only in OTA maintenance mode;
- captive-probe redirects for Android, Apple and Windows paths;
- unknown-path fallback to the OTA root page;
- unchanged `/health`, upload, finalize and abort paths.

## Remaining qualification gates

The current environment cannot perform the full PlatformIO Waveshare build. Therefore Build 194 is not hardware-qualified until the target system confirms:

1. clean PlatformIO compile;
2. clean upload;
3. stable USB serial without reset loop;
4. HUB75 initialization returns successfully;
5. native 64x64 output;
6. BLE advertising and official-app connect/disconnect;
7. normal command/media transfer;
8. physically triggered OTA AP;
9. captive-portal discovery/redirect behavior plus direct-IP fallback;
10. successful OTA happy path;
11. interrupted OTA leaves the previous valid firmware bootable.

The stable public baseline remains `0.5.2 / Build 190` until these development gates are closed.

## Build 194 captive-portal audit

- `DNSServer` is active only after the physical OTA trigger starts the maintenance AP.
- wildcard DNS resolves captive-connectivity hostnames to the SoftAP address;
- common Android, Apple and Windows probe paths redirect to `/`;
- unknown HTTP paths also redirect to `/`;
- `/health` remains an explicit diagnostic endpoint;
- direct access to `http://192.168.4.1/` remains supported if the client does not auto-open a portal;
- no external captive-portal or asynchronous web-server dependency is introduced.
