from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_FILES = [
    ROOT / "README.md",
    ROOT / "HISTORY.md",
    ROOT / "FUTURE-WORK.md",
    ROOT / "PROTOCOL.md",
    *sorted((ROOT / "docs").glob("*.md")),
]


def test_public_release_surface_has_no_current_cycle_intermediate_ids():
    text = "\n".join(p.read_text(encoding="utf-8") for p in PUBLIC_FILES)
    assert "0.6.0-" + "dev" not in text
    assert "0.6.0-" + "rc" not in text
    assert not re.search(r"\\bRC[12]\\b", text)
    assert not re.search(r"\\b(?:B|Build )?(?:19[0-9]|20[0-9]|21[0-9]|220|221)\\b", text)
