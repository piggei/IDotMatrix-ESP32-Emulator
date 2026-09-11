# Protocol captures

This directory contains selected excerpts from reverse-engineering sessions.

The captures are **experimental evidence**. [`PROTOCOL.md`](../../PROTOCOL.md) contains the current interpretation of that evidence. If a future discovery changes the interpretation of a field, the RAW capture should not be rewritten: update `PROTOCOL.md` instead.

The files have been reduced to the useful portions by removing periodic heap reports, RMT diagnostics and unrelated output. Significant bytes and ACKs are preserved unchanged.

`11-carousel-build94-mixed-content.txt` records the BUILD 94 diagnostic sequence that exposed a TEXT Bulk inside a 12-position Device Assets push and the resulting loss of carousel upload state. It intentionally does not assign an `imageIndex` to that TEXT asset because BUILD 94 did not log those header bytes for type 3.
