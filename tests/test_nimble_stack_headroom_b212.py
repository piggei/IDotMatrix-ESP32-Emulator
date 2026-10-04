from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIO = (ROOT / "platformio.ini").read_text()
INO = (ROOT / "src" / "IDotMatrix.ino").read_text()


def _env_block(name: str) -> str:
    marker = f"[env:{name}]"
    start = PIO.index(marker)
    next_env = PIO.find("\n[env:", start + len(marker))
    return PIO[start:] if next_env < 0 else PIO[start:next_env]


def test_b212_stack_hardening_and_b211_settle_are_preserved():
    assert '#define CAROUSEL_UPLOAD_SETTLE_MS 8000UL' in INO


def test_b212_waveshare_nimble_host_stack_is_8k():
    for env in (
        "waveshare_s3_rgbmatrix_64x64",
        "waveshare_s3_rgbmatrix_16x16",
        "waveshare_s3_rgbmatrix_32x32",
    ):
        block = _env_block(env)
        assert "-DIDOTMATRIX_USE_NIMBLE=1" in block
        assert "-DMYNEWT_VAL_NIMBLE_HOST_TASK_STACK_SIZE=8192" in block


def test_b212_non_waveshare_profiles_do_not_inherit_stack_override():
    for env in (
        "matrixportal_s3_hub75_64",
        "matrixportal_s3_hub75_64_icm20689",
        "ios_compat_esp32_ws2812_32",
        "esp32c3_ws2812_16",
    ):
        if f"[env:{env}]" in PIO:
            assert "MYNEWT_VAL_NIMBLE_HOST_TASK_STACK_SIZE" not in _env_block(env)
