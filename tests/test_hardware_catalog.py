from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOC = (ROOT / "docs" / "HARDWARE-MODULES.md").read_text(encoding="utf-8")
SUPPORT = (ROOT / "docs" / "HARDWARE-SUPPORT.md").read_text(encoding="utf-8")
README = (ROOT / "README.md").read_text(encoding="utf-8")

for name in [
    "adafruit-matrixportal-s3.jpg",
    "esp32-c3-supermini.jpg",
    "hub75-64x64-smd2121.jpg",
    "ws2812b-16x16-eco.png",
    "passive-buzzer-low-level-trigger.png",
    "ds3231-at24c32-vendor-specifications.png",
    "ds3231-at24c32-module.png",
    "gy521-mpu6050-module.png",
]:
    assert (ROOT / "assets" / "hardware" / name).is_file(), name

assert "Hardware-qualified" in DOC
assert "GY-521 module sold as MPU-6050" in DOC
assert "Implemented / awaiting physical qualification" in DOC
assert "WHO_AM_I" in DOC
assert "AT24C32" in DOC and "not currently used" in DOC
assert "HARDWARE-MODULES.md" in README
assert "GY-521" in SUPPORT
assert "adafruit-matrixportal-s3.jpg" in SUPPORT
assert "esp32-c3-supermini.jpg" in SUPPORT
assert "hub75-64x64-smd2121.jpg" in SUPPORT
assert "ws2812b-16x16-eco.png" in SUPPORT
print("hardware catalog regression: PASS")
