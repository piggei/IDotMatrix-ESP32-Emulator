from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def test_release_and_build_identity():
    assert '#define FW_RELEASE "0.6.0-rc.1"' in INO
    assert '#define FW_RELEASE_MAJOR 0' in INO
    assert '#define FW_RELEASE_MINOR 6' in INO
    assert '#define FW_BUILD 220' in INO
