# Protocol captures

This directory contains selected excerpts from reverse-engineering sessions.

The captures are **reverse-engineering evidence**. [`PROTOCOL.md`](../../PROTOCOL.md) contains the current interpretation of that evidence. If a future discovery changes the interpretation of a field, the RAW capture should not be rewritten: update `PROTOCOL.md` instead.

The files have been reduced to the useful portions by removing periodic heap reports, RMT diagnostics and unrelated output. Significant bytes and ACKs are preserved unchanged.

`11-carousel-mixed-content.txt` records the diagnostic sequence that exposed a TEXT Bulk inside a 12-position Device Assets push and the resulting loss of carousel upload state. It intentionally does not assign an `imageIndex` to that TEXT asset because that capture did not log those header bytes for type 3.

`12-preset-default-protocol.txt` records the Preset/Default protocol captures and subsequent hardware validation: volatile slots 14..19, mixed TEXT/GIF media, constant `timeSign=5`, the `06/02 <count> <slots...>` activation list, normal Bulk continuation markers on large Preset images, and content-aware TEXT timing.

`13-preset-default-validation.txt` records the hardware playback validation: mixed TEXT/GIF timing, five-image ordered playback, large multi-packet media, looping, and clean active-bank replacement.
