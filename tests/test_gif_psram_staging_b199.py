from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def test_waveshare_profiles_enable_guarded_transient_stage_only():
    assert PIO.count("-DIDOTMATRIX_GIF_PSRAM_STAGE_MAX_BYTES=2097152UL") == 3
    assert PIO.count("-DIDOTMATRIX_GIF_PSRAM_RESERVE_BYTES=4194304UL") == 3
    assert "#define IDOTMATRIX_GIF_PSRAM_STAGE_MAX_BYTES 0UL" in INO
    assert "#define IDOTMATRIX_GIF_PSRAM_RESERVE_BYTES 0UL" in INO


def test_stage_uses_external_psram_with_cap_reserve_and_largest_block_guards():
    assert "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in INO
    assert "heap_caps_get_free_size(caps)" in INO
    assert "heap_caps_get_largest_free_block(caps)" in INO
    assert "expectedSize > largestPsram" in INO
    assert "freePsram-expectedSize < reserve" in INO
    assert "heap_caps_malloc(expectedSize,caps)" in INO
    assert "heap_caps_free(gifStageData)" in INO


def test_decoder_callbacks_share_psram_and_littlefs_paths():
    assert "if(gifStageData && gifStageSize && gifStagePath==fname)" in INO
    assert "memcpy(pBuf,gifStageData+(size_t)pFile->iPos,(size_t)iLen)" in INO
    assert 'h->file=LittleFS.open(fname,"r")' in INO
    assert "h->file.seek((uint32_t)iPosition,SeekSet)" in INO
    assert "prepareGifStage(path,gifSize);" in INO


def test_stage_lifetime_extends_through_decoder_close():
    close_pos = INO.index("gif->close();")
    release_pos = INO.index("releaseGifStage();", close_pos)
    assert close_pos < release_pos
    assert "failure leaves the B198 file-backed decoder path intact" in INO


def test_staging_has_machine_readable_telemetry_and_total_first_frame_timing():
    for token in (
        "[GIFSTAGE] state=",
        'idotMemoryTelemetrySnapshot("gif.stage.before")',
        'idotMemoryTelemetrySnapshot("gif.stage.psram")',
        'idotMemoryTelemetrySnapshot("gif.stage.fallback")',
        'idotMemoryTelemetrySnapshot("gif.stage.released")',
        'idotMemoryTelemetryLatency("gif.stage.copy_us"',
    ):
        assert token in INO
    start = INO.index("bool startGIFFile(")
    first_timer = INO.index("gifTelemetryStartedAtUs = micros();", start)
    stage = INO.index("prepareGifStage(path,gifSize);", start)
    decoder_timer = INO.index("gifDecoderOpenStartedAtUs=micros();", start)
    assert first_timer < stage < decoder_timer


def test_b199_is_transient_only_not_cache_or_prefetch():
    assert "gifStageData" in INO
    assert "gifStagePeakBytes" in INO
    # B199 intentionally has no multi-entry cache/LRU/prefetch implementation.
    assert "GifCacheEntry" not in INO
    assert "gifCacheEntries" not in INO
    assert "prefetchGif" not in INO
