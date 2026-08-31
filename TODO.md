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

- [ ] Reproduce the original countdown icon/rotating hand animation
- [ ] Continue documenting unknown commands found on larger profiles
- [ ] Compare behaviour against original hardware captures
- [ ] Investigate a possible WLED integration/module
