from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _section(text: str, name: str) -> str:
    start = text.index(f"[env:{name}]")
    tail = text[start:]
    next_pos = tail.find("\n[env:", 1)
    return tail if next_pos < 0 else tail[:next_pos]


def test_waveshare_scaling_profiles_exist_and_select_expected_logical_size():
    profiles = {
        "waveshare_s3_rgbmatrix_16x16": 1,
        "waveshare_s3_rgbmatrix_32x32": 3,
        "waveshare_s3_rgbmatrix_64x64": 4,
    }
    for env, screen_type in profiles.items():
        sec = _section(PIO, env)
        assert f"IDOTMATRIX_SCREEN_TYPE={screen_type}" in sec
        assert "PHYSICAL_MATRIX_WIDTH=64" in sec
        assert "PHYSICAL_MATRIX_HEIGHT=64" in sec
        assert "board = esp32s3camlcd" in sec
        assert "board_build.arduino.memory_type = opi_opi" in sec
        assert "h2zero/NimBLE-Arduino @ 2.5.1" in sec
        assert "IDOTMATRIX_USE_NIMBLE=1" in sec
        assert "IDOTMATRIX_DEFAULT_OTA_ENABLED=1" in sec
        assert "partitions/idotmatrix_waveshare_s3_32mb_ota.csv" in sec


def test_waveshare_scaling_profiles_keep_identical_board_and_flash_policy():
    envs = [
        "waveshare_s3_rgbmatrix_16x16",
        "waveshare_s3_rgbmatrix_32x32",
        "waveshare_s3_rgbmatrix_64x64",
    ]
    required = [
        "platform-espressif32/releases/download/2026.05.50/platform-espressif32.zip",
        "board_build.flash_size = 32MB",
        "board_upload.flash_size = 32MB",
        "board_upload.maximum_size = 3145728",
        "board_build.flash_mode = dout",
        "BOARD_HAS_PSRAM",
        "IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX=1",
        "WAVESHARE_S3_PINOUT",
    ]
    for env in envs:
        sec = _section(PIO, env)
        for token in required:
            assert token in sec


def test_waveshare_scaling_profiles_do_not_enable_deferred_peripherals():
    for env in (
        "waveshare_s3_rgbmatrix_16x16",
        "waveshare_s3_rgbmatrix_32x32",
        "waveshare_s3_rgbmatrix_64x64",
    ):
        sec = _section(PIO, env)
        assert "IDOTMATRIX_DEFAULT_RTC_TYPE" not in sec
        assert "IDOTMATRIX_DEFAULT_ACCEL_DRIVER" not in sec
        assert "WLED_BUZZER" not in sec
        assert "I2S_SDPIN" not in sec
        assert "UM_SD_" not in sec


def _source_coord(physical_coord: int, logical_size: int) -> int:
    return physical_coord * logical_size // 64


def test_nearest_neighbour_16_to_64_is_exact_four_by_four_replication():
    mapped = [_source_coord(x, 16) for x in range(64)]
    assert mapped[0] == 0
    assert mapped[-1] == 15
    for source_x in range(16):
        assert mapped[source_x * 4:(source_x + 1) * 4] == [source_x] * 4


def test_nearest_neighbour_32_to_64_is_exact_two_by_two_replication():
    mapped = [_source_coord(x, 32) for x in range(64)]
    assert mapped[0] == 0
    assert mapped[-1] == 31
    for source_x in range(32):
        assert mapped[source_x * 2:(source_x + 1) * 2] == [source_x] * 2


def test_runtime_scaler_and_ble_identity_remain_generic():
    assert "const uint16_t sourceWidth = MATRIX_WIDTH;" in INO
    assert "const uint16_t sourceHeight = MATRIX_HEIGHT;" in INO
    assert "const uint16_t sx = (uint32_t)px * sourceWidth / targetWidth;" in INO
    assert "const uint16_t sy = (uint32_t)py * sourceHeight / targetHeight;" in INO
    assert "(char)IDOTMATRIX_SCREEN_TYPE" in INO
