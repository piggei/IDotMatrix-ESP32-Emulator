# TODO

## Completed / verified

- [x] Stable 16x16 hardware profile
- [x] BLE app discovery and core protocol handling
- [x] 32x32 (`0x03`) app-profile emulation
- [x] 64x64 (`0x04`) app-profile emulation
- [x] 64x64 cloud GIF reception and playback on classic ESP32
- [x] Large GIF streaming to LittleFS
- [x] Safe GIF RX/PLAY isolation and decoder lifecycle
- [x] 8x16 and 16x32 text glyph parsing
- [x] SimSun/SimHei app-side rasterization identified
- [x] Active buzzer on GPIO18 with non-blocking trill
- [x] Stopwatch/countdown colour behaviour
- [x] Event-driven OLED diagnostics

## Hardware validation

- [ ] Test firmware on a physical 32x32 iDotMatrix-compatible panel
- [ ] Test firmware on a physical 64x64 panel
- [ ] Capture/sniff traffic from an original 32x32 device when available
- [ ] Validate ESP32-C3 SuperMini as a compact target

## RTC / standalone operation

- [ ] Integrate optional RTC module
- [ ] Validate RTC state/power-loss detection
- [ ] Start directly in clock mode at power-on when a valid RTC is present
- [ ] Update RTC whenever the app sends a valid time sync
- [ ] Verify Alarm/Schedule operation after a cold boot without the phone

## Protocol / UI

- [ ] Continue documenting unknown commands found on larger profiles
- [ ] Compare behaviour against original hardware captures
- [ ] Investigate a possible WLED integration/module

## Reference implementation technical debt

- [ ] Move Alarm/Schedule GIF playback from the legacy full-RAM path to the validated LittleFS streaming/playback architecture
- [ ] Remove blocking delays from BLE callbacks and other latency-sensitive paths
- [ ] Move filesystem/bulk processing out of BLE callbacks where practical
- [ ] Verify the experimentally unconfirmed TEXT compatibility aliases `0x03` and `0x06` against original hardware/app captures
- [ ] Refine bulk ACK error semantics (`0x02`/`0x03`) using original-hardware captures
- [ ] Keep transport, filesystem, decoder and RAM media limits documented separately
