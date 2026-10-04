from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _slice(a: str, b: str) -> str:
    start = INO.index(a)
    return INO[start:INO.index(b, start)]


def test_waveshare_carousel_ble_branch_is_memory_only():
    bulk = _slice('bool processBulkPacket(', '// ======================================================\n// AUDIO / RHYTHM RENDERER')
    start = bulk.index('(type==1 || type==2 || type==3) && carouselUploadOpen')
    end = bulk.index('#else', start)
    route = bulk[start:end]
    assert 'beginCarouselBufferedImageTransfer(total,imageIndex,timeSign)' in route
    assert 'LittleFS.' not in route
    assert 'gifBulkFile' not in route
    assert 'String ' not in route


def test_buffered_carousel_chunks_copy_to_buffer_not_file():
    bulk = _slice('bool processBulkPacket(', '// ======================================================\n// AUDIO / RHYTHM RENDERER')
    assert 'if(bulk.carouselBuffered && carouselImageRxData' in bulk
    assert '::memcpy(carouselImageRxData+carouselImageRxWriteOffset,payload,useful);' in bulk
    # Existing GIF/TEXT filesystem streaming remains intact but buffered type-2 is excluded.
    assert 'if(bulk.carouselToFS || bulk.presetToFS || (type==1 && bulk.gifToFS))' in bulk


def test_loop_task_writes_any_buffered_carousel_before_commit():
    finalizer = _slice('void processDeferredAssetCommit()', 'void expireStalledTransfers')
    assert 'job.kind==DEFERRED_ASSET_CAROUSEL && carouselImageRxData' in finalizer
    assert 'carouselImageRxData' in finalizer
    assert 'LittleFS.open(tmp,"w")' in finalizer
    assert 'out.write(carouselImageRxData,job.size)' in finalizer
    write_at = finalizer.index('out.write(carouselImageRxData,job.size)')
    commit_at = finalizer.index('commitCarouselSlot(')
    assert write_at < commit_at


def test_buffer_lifetime_spans_deferred_commit():
    reset = _slice('void resetBulkTransfer(', 'bool queueDeferredAssetCommit')
    assert 'if(carouselImageRxData){ free(carouselImageRxData); carouselImageRxData=nullptr; }' in reset
    finalizer = _slice('void processDeferredAssetCommit()', 'void expireStalledTransfers')
    write_at = finalizer.index('out.write(carouselImageRxData,job.size)')
    reset_at = finalizer.index('resetBulkTransfer(false);')
    assert write_at < reset_at


def test_waveshare_requires_psram_and_preserves_reserve_for_carousel_rx():
    alloc = _slice('__attribute__((noinline)) uint8_t *allocateCarouselImageRxBuffer', '// Graffiti full-raster transfer')
    assert '#if defined(IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX)' in alloc
    assert 'heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)' in alloc
    wave = alloc[alloc.index('#if defined(IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX)'):alloc.index('#else')]
    assert 'IDOTMATRIX_GIF_PSRAM_RESERVE_BYTES' in wave
    assert 'heap_caps_get_largest_free_block' in wave
    assert 'malloc(bytes)' not in wave
    assert 'return (uint8_t*)malloc(bytes);' in alloc[alloc.index('#else'):]
