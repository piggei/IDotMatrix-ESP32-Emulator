from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _slice(a: str, b: str) -> str:
    start = INO.index(a)
    return INO[start:INO.index(b, start)]


def test_b209_type2_metadata_is_static_image_not_raw_only():
    assert '// 1=GIF, 2=static IMAGE (PNG or RGB24), 3=TEXT' in INO
    assert 'return dataType==3 ? "TEXT" : (dataType==2 ? "IMAGE" : "GIF");' in INO
    assert 'bool startsWithPNG(' in INO


def test_b210_carousel_type2_routes_by_context_not_rgb24_size():
    bulk = _slice('bool processBulkPacket(', '// ======================================================\n// AUDIO / RHYTHM RENDERER')
    assert 'if(type==2 && carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT' in bulk
    assert 'beginCarouselBufferedImageTransfer(total,imageIndex,timeSign)' in bulk
    helper = _slice('__attribute__((noinline)) bool beginCarouselBufferedImageTransfer', '// Graffiti full-raster transfer')
    assert 'bulk.carouselBuffered=true;' in helper
    assert 'BULK IMAGE BUFFERED: CAROUSEL slot=' in helper
    # The persistent Carousel route must not require RGB24 size or transient indices.
    route = bulk[bulk.index('if(type==2 && carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT'):]
    route = route[:route.index('if(type==2 && bulk.format=="RAW RGB" && !bulk.carouselBuffered)')]
    assert 'total==(uint32_t)NUM_LEDS*3UL' not in route
    assert 'imageIndex==12 || imageIndex==13' not in route
    # The release must not perform any filesystem work in the new type-2 BLE branch.
    assert 'LittleFS.' not in route
    assert 'String tmp' not in route


def test_b210_live_raw_path_remains_rgb24_only():
    bulk = _slice('bool processBulkPacket(', '// ======================================================\n// AUDIO / RHYTHM RENDERER')
    assert 'else if(type==2 && total==(uint32_t)NUM_LEDS*3UL) bulk.format="RAW RGB";' in bulk
    assert 'if(type==2 && bulk.format=="RAW RGB" && !bulk.carouselBuffered)' in bulk


def test_b209_static_image_playback_supports_rgb24_or_png_on_loop_task():
    assert 'bool loadCarouselImageFrame(const char *path, uint32_t expectedSize)' in INO
    image = _slice('bool loadCarouselImageFrame(', 'bool startCarouselSlot(uint8_t slot)')
    assert 'if(expectedSize==rawBytes) return loadCarouselRawFrame(path,expectedSize);' in image
    assert 'const bool ok=decodeSchedulePNG(f,expectedSize);' in image
    start = _slice('bool startCarouselSlot(uint8_t slot)', 'void updateCarousel(uint32_t now)')
    assert 'started=loadCarouselImageFrame(path.c_str(),m.mediaSize);' in start
    assert 'carousel.image.start_us' in start


def test_b209_type2_size_restrictions_removed_from_recovery_and_commit():
    recover = _slice('void recoverCarouselSlot(uint8_t slot)', 'void loadCarousel()')
    commit = _slice('bool commitCarouselSlot(', 'void processDeferredAssetCommit()')
    assert 'm.dataType==2 && m.mediaSize!=(uint32_t)NUM_LEDS*3UL' not in recover
    assert 'dataType==2 && size!=(uint32_t)NUM_LEDS*3UL' not in commit


def test_b209_removes_heavy_b208_nimble_stack_diagnostics():
    bulk = _slice('bool processBulkPacket(', '// ======================================================\n// AUDIO / RHYTHM RENDERER')
    assert '[CAROUSEL TYPE2 HDR]' not in bulk
    assert '[CAROUSEL TYPE2 PAYLOAD]' not in bulk
    assert 'char diag[256]' not in bulk
    assert 'char diag[320]' not in bulk
    assert 'char hex[32*3+1]' not in bulk
    assert 'type2PayloadProbeDone' not in INO


def test_b209_prefetch_skips_static_image_without_scanning_farther():
    schedule = _slice('void scheduleCarouselGifPrefetch', 'bool beginCarouselGifPrefetch')
    assert 'm.dataType==2 ? "next_image"' in schedule
