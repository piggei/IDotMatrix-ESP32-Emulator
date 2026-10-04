from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _slice(a: str, b: str) -> str:
    start = INO.index(a)
    return INO[start:INO.index(b, start)]


def test_b210_type2_ble_branch_is_memory_only():
    bulk = _slice('bool processBulkPacket(', '// ======================================================\n// AUDIO / RHYTHM RENDERER')
    start = bulk.index('if(type==2 && carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT')
    end = bulk.index('if(type==2 && bulk.format=="RAW RGB" && !bulk.carouselBuffered)', start)
    route = bulk[start:end]
    assert 'beginCarouselBufferedImageTransfer(total,imageIndex,timeSign)' in route
    assert 'LittleFS.' not in route
    assert 'gifBulkFile' not in route
    assert 'String ' not in route


def test_b210_type2_chunks_copy_to_buffer_not_file():
    bulk = _slice('bool processBulkPacket(', '// ======================================================\n// AUDIO / RHYTHM RENDERER')
    assert 'if(type==2 && bulk.carouselBuffered && carouselImageRxData' in bulk
    assert '::memcpy(carouselImageRxData+carouselImageRxWriteOffset,payload,useful);' in bulk
    # Existing GIF/TEXT filesystem streaming remains intact but buffered type-2 is excluded.
    assert 'if(bulk.carouselToFS || bulk.presetToFS || (type==1 && bulk.gifToFS))' in bulk


def test_b210_loop_task_writes_buffer_before_commit():
    finalizer = _slice('void processDeferredAssetCommit()', 'void expireStalledTransfers')
    assert 'job.kind==DEFERRED_ASSET_CAROUSEL && job.dataType==2' in finalizer
    assert 'carouselImageRxData' in finalizer
    assert 'LittleFS.open(tmp,"w")' in finalizer
    assert 'out.write(carouselImageRxData,job.size)' in finalizer
    write_at = finalizer.index('out.write(carouselImageRxData,job.size)')
    commit_at = finalizer.index('commitCarouselSlot(')
    assert write_at < commit_at


def test_b210_buffer_lifetime_spans_deferred_commit():
    reset = _slice('void resetBulkTransfer(', 'bool queueDeferredAssetCommit')
    assert 'if(carouselImageRxData){ free(carouselImageRxData); carouselImageRxData=nullptr; }' in reset
    finalizer = _slice('void processDeferredAssetCommit()', 'void expireStalledTransfers')
    write_at = finalizer.index('out.write(carouselImageRxData,job.size)')
    reset_at = finalizer.index('resetBulkTransfer(false);')
    assert write_at < reset_at


def test_b210_waveshare_prefers_psram_for_static_image_rx():
    alloc = _slice('__attribute__((noinline)) uint8_t *allocateCarouselImageRxBuffer', '// Graffiti full-raster transfer')
    assert '#if defined(IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX)' in alloc
    assert 'heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)' in alloc
    assert 'return (uint8_t*)malloc(bytes);' in alloc
