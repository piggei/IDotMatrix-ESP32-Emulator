from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INI = (ROOT / "platformio.ini").read_text(encoding="utf-8")
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
MEM_H = (ROOT / "src" / "IDotMatrixMemoryTelemetry.h").read_text(encoding="utf-8")
MEM_CPP = (ROOT / "src" / "IDotMatrixMemoryTelemetry.cpp").read_text(encoding="utf-8")
OTA = (ROOT / "src" / "IDotMatrixOta.cpp").read_text(encoding="utf-8")


def section(name: str) -> str:
    start = INI.index(f"[env:{name}]")
    next_env = INI.find("\n[env:", start + 1)
    return INI[start:] if next_env < 0 else INI[start:next_env]


def test_memory_telemetry_is_enabled_only_for_waveshare_profiles():
    for env in (
        "waveshare_s3_rgbmatrix_16x16",
        "waveshare_s3_rgbmatrix_32x32",
        "waveshare_s3_rgbmatrix_64x64",
    ):
        assert "-DIDOTMATRIX_MEMORY_TELEMETRY=1" in section(env)

    for env in (
        "matrixportal_s3_hub75_64",
        "esp32c3_ws2812_16",
    ):
        assert "-DIDOTMATRIX_MEMORY_TELEMETRY=1" not in section(env)


def test_structured_snapshot_reports_internal_dma_and_psram_pools():
    assert "MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT" in MEM_CPP
    assert "MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA" in MEM_CPP
    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in MEM_CPP
    for field in (
        "int_free=", "int_min=", "int_largest=",
        "dma_free=", "dma_min=", "dma_largest=",
        "psram_total=", "psram_free=", "psram_min=", "psram_largest=",
    ):
        assert field in MEM_CPP
    assert 'Serial.print("[MEM] t_ms=")' in MEM_CPP
    assert 'Serial.print("[LAT] t_ms=")' in MEM_CPP


def test_non_measurement_profiles_compile_to_noop_telemetry_calls():
    assert "#define IDOTMATRIX_MEMORY_TELEMETRY 0" in MEM_H
    assert "inline void idotMemoryTelemetrySnapshot" in MEM_H
    assert "inline void idotMemoryTelemetryLatency" in MEM_H


def test_b196_covers_boot_media_and_ota_measurement_points():
    for tag in (
        '"setup.begin"',
        '"setup.before_logical_buffers"',
        '"setup.after_logical_buffers"',
        '"setup.after_storage"',
        '"setup.before_hub75"',
        '"setup.after_hub75"',
        '"setup.after_ble"',
        '"setup.after_ota_arm"',
        '"runtime.periodic"',
        '"text.live.before_parse"',
        '"gif.live.before_open"',
        '"gif.live.first_frame"',
        '"carousel.before_start"',
        '"schedule.png.before_decode"',
        '"raw.before_alloc"',
        '"graffiti.before_alloc"',
    ):
        assert tag in INO

    for tag in (
        '"ota.ap.before_wifi"',
        '"ota.ap.ready"',
        '"ota.upload.before_begin"',
        '"ota.upload.after_begin"',
        '"ota.upload.complete"',
        '"ota.upload.aborted"',
    ):
        assert tag in OTA


def test_b196_records_latency_without_changing_media_storage_policy():
    for tag in (
        '"text.live.parse_render_us"',
        '"gif.live.open_us"',
        '"gif.live.first_frame_us"',
        '"gif.carousel.first_frame_us"',
        '"carousel.gif.start_us"',
        '"schedule.png.decode_us"',
    ):
        assert tag in INO

    # Measurement-only telemetry must preserve the existing malloc/file-backed paths.
    assert "framebuffer = (CRGB*)malloc(LOGICAL_FRAME_BYTES);" in INO
    assert "gifFrame = (CRGB*)malloc(LOGICAL_FRAME_BYTES);" in INO
    assert "scheduleSavedFrame = (CRGB*)malloc(LOGICAL_FRAME_BYTES);" in INO
    assert "bool opened=gif->open(path,GIFOpenFile,GIFCloseFile,GIFReadFile,GIFSeekFile,GIFDraw);" in INO
