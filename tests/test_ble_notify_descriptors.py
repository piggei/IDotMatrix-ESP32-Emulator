from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text()


def test_notify_characteristics_rely_on_framework_cccd():
    assert "#include <BLE2902.h>" not in INO
    assert "new BLE2902" not in INO
    assert "BLECharacteristic::PROPERTY_NOTIFY" in INO


if __name__ == "__main__":
    test_notify_characteristics_rely_on_framework_cccd()
    print("BLE notify descriptor cleanup: PASS")
