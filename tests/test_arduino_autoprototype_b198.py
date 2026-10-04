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
