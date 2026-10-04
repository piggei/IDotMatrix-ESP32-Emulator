from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _between(start: str, end: str) -> str:
    a = INO.index(start)
    b = INO.index(end, a)
    return INO[a:b]


def test_has_deferred_asset_commit_state():
    assert "enum DeferredAssetCommitKind" in INO
    assert "DEFERRED_ASSET_CAROUSEL" in INO
    assert "DEFERRED_ASSET_PRESET" in INO
    assert "DeferredAssetCommitState" in INO


def test_preset_and_carousel_commit_run_only_in_loop_side_finalizer():
    bulk = _between("bool processBulkPacket", "// ======================================================\n// AUDIO / RHYTHM RENDERER")
    finalizer = _between("void processDeferredAssetCommit", "void expireStalledTransfers")
    assert "commitCarouselSlot(" not in bulk
    assert "commitPresetSlot(" not in bulk
    assert "commitCarouselSlot(" in finalizer
    assert "commitPresetSlot(" in finalizer
    assert "gifBulkFile.flush();" in finalizer
    assert "gifBulkFile.close();" in finalizer
    assert "LittleFS.remove(carouselTempFileName" in finalizer
    assert "LittleFS.remove(presetTempFileName" in finalizer


def test_final_ack_is_deferred_until_after_runtime_state_reset():
    finalizer = _between("void processDeferredAssetCommit", "void expireStalledTransfers")
    reset_at = finalizer.index("resetBulkTransfer(false);")
    ack_at = finalizer.index("sendTransferAck(job.dataType,0x03);")
    assert reset_at < ack_at

    bulk = _between("bool processBulkPacket", "// ======================================================\n// AUDIO / RHYTHM RENDERER")
    branch = bulk[bulk.index("if(bulk.carouselToFS || bulk.carouselBuffered || bulk.presetToFS){"):]
    branch = branch[:branch.index("} else {")]
    assert "queueDeferredAssetCommit(" in branch
    assert "sendTransferAck" not in branch
    assert "commitPresetSlot" not in branch
    assert "commitCarouselSlot" not in branch
    assert "LittleFS.remove" not in branch


def test_loop_processes_deferred_commit_before_transfer_timeouts():
    loop = INO[INO.index("void loop(){"):]
    commit_at = loop.index("processDeferredAssetCommit();")
    timeout_at = loop.index("expireStalledTransfers(now);")
    assert commit_at < timeout_at
    assert "bulk.active && !deferredAssetCommit.active" in INO


def test_disconnect_preserves_fully_received_pending_commit():
    disconnect = _between("void handleBleDisconnected", "#if IDOTMATRIX_USE_NIMBLE")
    assert "if(!deferredAssetCommit.active) resetBulkTransfer(true);" in disconnect


def test_keeps_memory_telemetry_around_loop_side_commit():
    finalizer = _between("void processDeferredAssetCommit", "void expireStalledTransfers")
    assert '"fs.commit.carousel.before"' in finalizer
    assert '"fs.commit.carousel.after"' in finalizer
    assert '"fs.commit.preset.before"' in finalizer
    assert '"fs.commit.preset.after"' in finalizer
