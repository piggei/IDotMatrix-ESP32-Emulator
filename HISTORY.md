# Release History

## 0.5.0 / Build 162

Final 0.5.0 release of the standalone iDotMatrix ESP32 Emulator.

Highlights:

- hardware-qualified Adafruit MatrixPortal ESP32-S3 + 64x64 HUB75 target;
- hardware-qualified ESP32-C3 + 16x16 WS2812 target;
- logical 16x16, 32x32 and 64x64 display profiles with resolution-independent output scaling;
- Clock, Countdown, Stopwatch and Scoreboard rendering aligned with original-device references;
- TEXT support for 16/32/64-pixel glyph families, scrolling, paging and multiline static layouts;
- image/GIF playback, Graffiti/DIY, persistent Device Assets Carousel and volatile Preset/Default playback;
- Alarm and Program/Schedule multipart media handling with CRC32 validation;
- five LEVEL and five FFT Audio/Rhythm effects with dedicated audio stream framing;
- verified TEXT/Preset/Carousel display ownership isolation;
- LittleFS media storage and transactional replacement paths;
- cleaned public documentation and release metadata.

The 16x16 Stopwatch animation was revalidated frame-by-frame against the supplied original-device reference video before this release. No additional visual change was required.

## 0.4.0 / Build 118

Previous stable standalone emulator release. It established the original WS2812-focused baseline before the larger-display, multipart-media, Preset and MatrixPortal/HUB75 work included in 0.5.0.
