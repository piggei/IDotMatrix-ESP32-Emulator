from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _bulk():
    a = INO.index("bool processBulkPacket(")
    b = INO.index("// ======================================================\n// AUDIO / RHYTHM RENDERER", a)
    return INO[a:b]


def test_waveshare_buffers_all_persistent_carousel_types():
    bulk=_bulk()
    assert '(type==1 || type==2 || type==3) && carouselUploadOpen' in bulk
    assert 'beginCarouselBufferedImageTransfer(total,imageIndex,timeSign)' in bulk
    assert 'if(bulk.carouselBuffered && carouselImageRxData' in bulk


def test_waveshare_compiles_out_direct_carousel_fs_open_paths():
    bulk=_bulk()
    guard='#if !defined(IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX)'
    assert guard + '\n    if(type==3 && !bulk.presetToFS' in bulk
    assert guard + '\n      if(carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT' in bulk
    # The direct persistent opens exist only for non-Waveshare compatibility.
    assert 'BULK GIF STORAGE: CAROUSEL slot=' in bulk
    assert 'BULK TEXT STORAGE: CAROUSEL slot=' in bulk
    # Live/preview GIF reception intentionally remains available on Waveshare.
    assert 'LittleFS.open(rxPath,"w")' in bulk


def test_deferred_loop_task_writes_buffer_for_gif_image_or_text():
    a=INO.index('void processDeferredAssetCommit()')
    b=INO.index('void expireStalledTransfers',a)
    finalizer=INO[a:b]
    assert 'job.kind==DEFERRED_ASSET_CAROUSEL && carouselImageRxData' in finalizer
    assert 'out.write(carouselImageRxData,job.size)' in finalizer
    assert 'commitCarouselSlot(job.localSlot,job.dataType' in finalizer
    assert finalizer.index('out.write(carouselImageRxData,job.size)') < finalizer.index('commitCarouselSlot(')


def test_preserves_non_waveshare_streaming_code_for_existing_targets():
    bulk=_bulk()
    assert '#if !defined(IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX)' in bulk
    assert 'BULK GIF STORAGE: CAROUSEL slot=' in bulk
    assert 'BULK TEXT STORAGE: CAROUSEL slot=' in bulk
