from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _carousel_handler():
    start = INO.index("bool handleCarouselCommand(const uint8_t *data, size_t len) {")
    end = INO.index("// ======================================================\n// PRESET / DEFAULT", start)
    return INO[start:end]


def _deferred_setup():
    start = INO.index("void processDeferredCarouselBankSetup() {")
    end = INO.index("bool handleCarouselCommand", start)
    return INO[start:end]


def _update_carousel():
    start = INO.index("void updateCarousel(uint32_t now) {")
    end = INO.index("bool commitCarouselSlot", start)
    return INO[start:end]


def test_device_assets_bank_setup_establishes_carousel_view_intent():
    handler = _carousel_handler()
    assert "const bool viewIntent=true;" in handler
    assert "queueDeferredCarouselBankSetup(data+5,count,viewIntent)" in handler


def test_deferred_setup_cannot_clear_later_assets_view_request():
    setup = _deferred_setup()
    assert "carouselEnterRequested=queuedViewIntent || carouselEnterRequested;" in setup


def test_committed_bank_starts_when_view_intent_is_set():
    update = _update_carousel()
    assert "bool canStart=carouselEnterRequested && nextCarouselSlot(-1)>=0;" in update
    assert "if(canStart) carouselStartPending=true;" in update
    assert "if(first>=0) startCarouselSlot((uint8_t)first);" in update


def test_starting_carousel_releases_active_preset_owner():
    start = INO.index("bool startCarouselSlot(uint8_t slot) {")
    end = INO.index("void updateCarousel", start)
    body = INO[start:end]
    assert "if(presetActive) stopPresetPlayback();" in body
