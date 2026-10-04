from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
PART = ROOT / "partitions" / "idotmatrix_waveshare_s3_32mb_ota.csv"


def _section(text: str, name: str) -> str:
    start = text.index(f"[env:{name}]")
    tail = text[start:]
    next_pos = tail.find("\n[env:", 1)
    return tail if next_pos < 0 else tail[:next_pos]


def test_waveshare_environment_declares_n32r16_resources():
    sec = _section(PIO, "waveshare_s3_rgbmatrix_64x64")
    assert "board = esp32s3camlcd" in sec
    assert "board_build.arduino.memory_type = opi_opi" in sec
    assert "board_build.flash_size = 32MB" in sec
    assert "BOARD_HAS_PSRAM" in sec
    assert "IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX=1" in sec
    assert "IDOTMATRIX_SCREEN_TYPE=4" in sec
    assert "PHYSICAL_MATRIX_WIDTH=64" in sec
    assert "PHYSICAL_MATRIX_HEIGHT=64" in sec


def test_waveshare_uses_wled_qualified_framework_baseline():
    sec = _section(PIO, "waveshare_s3_rgbmatrix_64x64")
    assert "tasmota/platform-espressif32/releases/download/2026.05.50/platform-espressif32.zip" in sec
    assert "board_build.flash_mode = dout" in sec
    assert "board_build.flash_mode = opi" not in sec


def test_waveshare_first_bringup_does_not_enable_future_peripherals():
    sec = _section(PIO, "waveshare_s3_rgbmatrix_64x64")
    assert "IDOTMATRIX_DEFAULT_RTC_TYPE" not in sec
    assert "IDOTMATRIX_DEFAULT_ACCEL_DRIVER" not in sec
    assert "WLED_BUZZER" not in sec
    assert "I2S_SDPIN" not in sec
    assert "UM_SD_" not in sec


def test_waveshare_hub75_pinout_matches_official_wled_mapping():
    assert "4, 5, 6, 7, 15, 16" in INO
    assert "18, 8, 3, 42, 9" in INO
    assert "40, 2, 41" in INO
    assert "IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX" in INO


def test_waveshare_partition_geometry_matches_wled_32mb_baseline():
    sec = _section(PIO, "waveshare_s3_rgbmatrix_64x64")
    assert "partitions/idotmatrix_waveshare_s3_32mb_ota.csv" in sec
    assert "board_upload.maximum_size = 3145728" in sec
    text = PART.read_text(encoding="utf-8")
    assert "0x10000,   0x300000" in text
    assert "0x310000,  0x300000" in text
    assert "0x610000,  0x19E0000" in text
    assert "coredump" in text


def test_waveshare_hub75_has_boot_checkpoints():
    assert "BOOT CHECKPOINT: entering HUB75 initialization" in INO
    assert "BOOT CHECKPOINT: returned from HUB75 initialization" in INO


def test_waveshare_uses_pinned_nimble_backend():
    sec = _section(PIO, "waveshare_s3_rgbmatrix_64x64")
    assert "h2zero/NimBLE-Arduino @ 2.5.1" in sec
    assert "IDOTMATRIX_USE_NIMBLE=1" in sec
    assert "#include <NimBLEDevice.h>" in INO
    assert "NimBLECharacteristicCallbacks" in INO
    assert "NimBLEConnInfo&" in INO
    assert "NIMBLE_PROPERTY::WRITE" in INO
    assert "NimBLEDevice::startAdvertising()" in INO


def test_legacy_ble_backend_remains_available_for_existing_profiles():
    assert "#include <BLEDevice.h>" in INO
    assert "BLECharacteristic::PROPERTY_WRITE" in INO
    assert "#if IDOTMATRIX_USE_NIMBLE" in INO
