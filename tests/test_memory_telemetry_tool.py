from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "summarize_memory_telemetry.py"


def test_memory_telemetry_log_summarizer():
    sample = """\
[MEM] t_ms=1 tag=setup.begin int_free=100 int_min=95 int_largest=80 dma_free=90 dma_min=85 dma_largest=70 psram_total=1000 psram_free=900 psram_min=880 psram_largest=850
[MEM] t_ms=2 tag=setup.begin int_free=90 int_min=85 int_largest=70 dma_free=80 dma_min=75 dma_largest=60 psram_total=1000 psram_free=850 psram_min=830 psram_largest=800
[LAT] t_ms=3 tag=gif.live.first_frame_us us=1200
[LAT] t_ms=4 tag=gif.live.first_frame_us us=1800
"""
    with tempfile.TemporaryDirectory() as td:
        log = Path(td) / "serial.log"
        log.write_text(sample, encoding="utf-8")
        result = subprocess.run([sys.executable, str(TOOL), str(log)], check=True, text=True, capture_output=True)
    out = result.stdout
    assert "setup.begin,2,90,70,80,60,850,800" in out
    assert "gif.live.first_frame_us,2,1200,1500,1800" in out
    assert "int_free=90" in out
