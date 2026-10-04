from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def test_deferred_commit_helper_signature_uses_arduino_safe_primitive_type():
    assert "bool queueDeferredAssetCommit(uint8_t kindValue" in INO
    assert "bool queueDeferredAssetCommit(DeferredAssetCommitKind" not in INO
    assert "static_cast<DeferredAssetCommitKind>(kindValue)" in INO


def test_custom_enum_is_still_used_for_stored_state():
    assert "enum DeferredAssetCommitKind : uint8_t" in INO
    assert "DeferredAssetCommitKind kind = DEFERRED_ASSET_NONE;" in INO


def test_custom_runtime_helpers_do_not_expose_custom_types_in_signatures():
    ino = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
    assert "bool gifCachePathEquals(const GifCacheEntry" not in ino
    assert "bool gifCachePathEquals(uint8_t index" in ino
    assert "CarouselGifPrefetchState" not in "\n".join(
        line for line in ino.splitlines() if line.lstrip().startswith(("bool ", "void ", "int8_t ", "String "))
    )
    assert "String carouselFileName(uint8_t slot, uint8_t dataType);" in ino
    signatures = "\n".join(line for line in ino.splitlines() if line.lstrip().startswith(("bool ", "void ", "int8_t ", "String ", "uint32_t ")) )
    assert "(CarouselBankTxnManifest" not in signatures
    assert "(DeferredCarouselBankSetupState" not in signatures
