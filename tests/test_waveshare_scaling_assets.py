from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs" / "qualification-assets" / "waveshare-scaling"


def _png_size(path: Path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    assert data[12:16] == b"IHDR"
    return struct.unpack(">II", data[16:24])


def test_scaling_assets_match_logical_profiles():
    for n in (16, 32, 64):
        path = ASSETS / f"waveshare-scaling-{n}x{n}.png"
        assert path.exists()
        assert _png_size(path) == (n, n)


def test_scaling_asset_readme_documents_expected_replication():
    text = (ASSETS / "README.md").read_text(encoding="utf-8")
    assert "4x4 physical pixels" in text
    assert "2x2 physical pixels" in text
    assert "exact 1:1 output" in text
