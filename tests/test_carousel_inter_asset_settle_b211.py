from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def test_b211_device_assets_settle_window_bridges_observed_gap():
    assert '#define CAROUSEL_UPLOAD_SETTLE_MS 8000UL' in INO
    assert 'inter-asset gap' in INO
    assert 'emulator policy' in INO


def test_b211_carousel_routing_still_depends_on_open_upload_window():
    # The fix is deliberately narrow: extend the proven Device Assets
    # classification window rather than changing GIF/TEXT wire semantics.
    assert 'if(type==3 && !bulk.presetToFS && carouselUploadOpen' in INO
    assert 'if(carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT' in INO
    assert 'if(type==2 && carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT' in INO


def test_b211_settle_still_requires_no_active_bulk_transfer():
    marker = 'if(carouselUploadOpen && carouselLastAssetCommitAt && !bulk.active &&'
    assert marker in INO
    assert '>=CAROUSEL_UPLOAD_SETTLE_MS' in INO
