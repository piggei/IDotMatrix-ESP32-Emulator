from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def test_waveshare_profiles_enable_bounded_cache_only():
    assert PIO.count("-DIDOTMATRIX_GIF_PSRAM_CACHE_MAX_BYTES=1048576UL") == 3
    assert PIO.count("-DIDOTMATRIX_GIF_PSRAM_CACHE_ENTRY_MAX_BYTES=524288UL") == 3
    assert PIO.count("-DIDOTMATRIX_GIF_PSRAM_CACHE_MAX_ENTRIES=12") == 3
    assert "#define IDOTMATRIX_GIF_PSRAM_CACHE_MAX_BYTES 0UL" in INO
    assert "#define IDOTMATRIX_GIF_PSRAM_CACHE_ENTRY_MAX_BYTES 0UL" in INO
    assert "#define IDOTMATRIX_GIF_PSRAM_CACHE_MAX_ENTRIES 0" in INO


def test_cache_identity_is_path_size_crc_and_lru_is_bounded():
    assert "e.size!=size || e.crc!=crc || !gifCachePathEquals(i,path)" in INO
    assert "gifCacheBytes<=budget-incoming" in INO
    assert "gifCacheLruVictim()" in INO
    assert "e.lastUse=++gifCacheClock" in INO
    assert "gifCacheBytes+=e.size" in INO
    assert "gifCacheBytes-=oldSize" in INO


def test_active_cache_entry_is_protected_until_decoder_close():
    assert "if((int8_t)index==gifCacheActiveIndex) return false;" in INO
    close_pos = INO.index("gif->close();")
    unpin_pos = INO.index("gifCacheActiveIndex=-1;", close_pos)
    assert close_pos < unpin_pos


def test_cache_adopts_cold_stage_without_second_copy():
    owned = INO.index("bool gifCacheInsertOwnedBuffer(")
    assign = INO.index("e.data=data;", owned)
    adopt = INO.index("bool gifCacheAdoptStage(", assign)
    transfer = INO.index("gifCacheInsertOwnedBuffer(path,gifStageSize,crc,gifStageData", adopt)
    clear = INO.index("gifStageData=nullptr;", transfer)
    assert assign < adopt < transfer < clear
    assert "heap_caps_malloc" not in INO[owned:INO.index("bool prepareGifStage", adopt)]


def test_cache_hits_skip_disk_crc_for_known_carousel_and_preset_gifs():
    assert "!gifCacheMatches(path.c_str(),m.mediaSize,m.mediaCRC) && !fileMatchesMedia" in INO
    assert "gifCacheMatches(path.c_str(),m.mediaSize,m.mediaCRC) || fileMatchesMedia" in INO


def test_mutating_paths_invalidate_cache_identity():
    assert "gifCacheInvalidatePath(GIF_PLAY_FILE);" in INO
    assert "gifCacheInvalidatePath(EVENT_GIF_PLAY_FILE);" in INO
    assert "gifCacheInvalidatePath(dst.c_str());" in INO
    assert "gifCacheInvalidatePath(gifPath.c_str());" in INO


def test_cache_has_machine_readable_diagnostics():
    for token in (
        "[GIFCACHE] state=",
        'reportGifCache("hit"',
        'reportGifCache("miss"',
        'reportGifCache("insert"',
        '"evict"',
        '"invalidate"',
        'idotMemoryTelemetryLatency("gif.cache.hit_us"',
        'idotMemoryTelemetrySnapshot("gif.cache.insert")',
    ):
        assert token in INO
