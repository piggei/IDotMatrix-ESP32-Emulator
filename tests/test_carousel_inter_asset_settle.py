from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def test_device_assets_settle_window_bridges_observed_gap():
    assert '#define CAROUSEL_UPLOAD_SETTLE_MS 8000UL' in INO
    assert 'inter-asset gap' in INO
    assert 'emulator policy' in INO


def test_carousel_routing_still_depends_on_open_upload_window():
    # The fix is deliberately narrow: extend the proven Device Assets
    # classification window rather than changing GIF/TEXT wire semantics.
    assert '(type==1 || type==2 || type==3) && carouselUploadOpen' in INO
    assert 'imageIndex<CAROUSEL_SLOT_COUNT' in INO
    assert 'CAROUSEL_UPLOAD_SETTLE_MS' in INO


def test_settle_still_requires_no_active_bulk_transfer():
    marker = 'if(carouselUploadOpen && carouselLastAssetCommitAt && !bulk.active &&'
    assert marker in INO
    assert '>=CAROUSEL_UPLOAD_SETTLE_MS' in INO
