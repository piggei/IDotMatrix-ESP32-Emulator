from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def test_prefetch_enabled_only_on_waveshare_profiles():
    assert PIO.count("-DIDOTMATRIX_CAROUSEL_GIF_PREFETCH=1") == 3
    assert "#define IDOTMATRIX_CAROUSEL_GIF_PREFETCH 0" in INO
    assert "#define IDOTMATRIX_CAROUSEL_GIF_PREFETCH_DELAY_MS 250UL" in INO
    assert "#define IDOTMATRIX_CAROUSEL_GIF_PREFETCH_CHUNK_BYTES 4096U" in INO


def test_prefetch_is_one_immediate_carousel_item_only():
    schedule = INO[INO.index("void scheduleCarouselGifPrefetch"):INO.index("bool beginCarouselGifPrefetch")]
    assert "int8_t next=nextCarouselSlot((int8_t)currentSlot);" in schedule
    assert "m.dataType!=1" in schedule
    assert 'skipCarouselGifPrefetch(m.dataType==3 ? "next_text" : (m.dataType==2 ? "next_image" : "metadata"))' in schedule
    assert "for(" not in schedule and "while(" not in schedule


def test_prefetch_is_incremental_and_rendering_gets_loop_priority():
    service = INO[INO.index("void serviceCarouselGifPrefetch"):INO.index("void destroyGIFDecoder")]
    assert "IDOTMATRIX_CAROUSEL_GIF_PREFETCH_CHUNK_BYTES" in service
    assert "carouselGifPrefetch.file.read" in service
    assert "crc32Update(carouselGifPrefetch.runningCRC" in service
    loop = INO[INO.index("void loop(){"):]
    gif_update = loop.index("updateGIF();")
    text_update = loop.index("updateTextAnimation();")
    prefetch = loop.index("serviceCarouselGifPrefetch(now);")
    assert gif_update < prefetch
    assert text_update < prefetch


def test_prefetch_validates_crc_before_cache_ownership_transfer():
    finish = INO[INO.index("void finishCarouselGifPrefetch"):INO.index("void serviceCarouselGifPrefetch")]
    crc_check = finish.index("calc!=carouselGifPrefetch.expectedCRC")
    cache_insert = finish.index("gifCacheInsertOwnedBuffer")
    ownership = finish.index("carouselGifPrefetch.data=nullptr")
    assert crc_check < cache_insert < ownership
    assert '"prefetch",&inserted' in finish


def test_prefetch_obeys_psram_reserve_and_cache_entry_cap():
    assert "m.mediaSize>(uint32_t)IDOTMATRIX_GIF_PSRAM_CACHE_ENTRY_MAX_BYTES" in INO
    assert "carouselGifPrefetch.size>largestPsram" in INO
    assert "freePsram-carouselGifPrefetch.size<reserve" in INO
    assert "heap_caps_malloc(carouselGifPrefetch.size,caps)" in INO


def test_prefetch_is_cancelled_on_advance_stop_or_stale_metadata():
    start_pos = INO.index("bool startCarouselSlot(uint8_t slot) {")
    start = INO[start_pos:INO.index("void updateCarousel", start_pos)]
    stop_pos = INO.index("void stopCarouselPlayback() {")
    stop = INO[stop_pos:INO.index("int8_t nextCarouselSlot(int8_t current) {", stop_pos)]
    service = INO[INO.index("void serviceCarouselGifPrefetch"):INO.index("void destroyGIFDecoder")]
    assert 'cancelCarouselGifPrefetch("advance")' in start
    assert 'cancelCarouselGifPrefetch("stop")' in stop
    assert 'cancelCarouselGifPrefetch("stale")' in service


def test_prefetch_has_machine_readable_diagnostics():
    for token in (
        "[GIFPREFETCH] state=",
        'reportCarouselGifPrefetch("scheduled"',
        'reportCarouselGifPrefetch("begin"',
        'reportCarouselGifPrefetch("cached"',
        'idotMemoryTelemetrySnapshot("gif.prefetch.begin")',
        'idotMemoryTelemetrySnapshot("gif.prefetch.cached")',
        'idotMemoryTelemetryLatency("gif.prefetch.total_us"',
    ):
        assert token in INO
