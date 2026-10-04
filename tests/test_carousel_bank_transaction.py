from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def test_has_persistent_carousel_bank_journal_and_rename_backups():
    assert "CAROUSEL_BANK_TXN_MAGIC" in INO
    assert 'CAROUSEL_BANK_TXN_FILE = "/carbank.txn"' in INO
    assert 'CAROUSEL_BANK_TXN_TMP_FILE = "/carbank.tmp"' in INO
    assert '".old.txt" : ".old.gif"' in INO
    assert "touchedMask" in INO
    assert "mediaPresentMask" in INO


def test_manifest_is_written_before_old_media_are_renamed():
    begin = INO[INO.index("bool beginCarouselBankTransaction"):INO.index("bool finalizeCarouselBankTransaction")]
    assert begin.index("persistCarouselBankTxnManifest()") < begin.index("LittleFS.rename(src,bak)")
    assert 'rollbackCarouselBankTransaction("backup_failed")' in begin


def test_manifest_removal_is_commit_point_before_backup_cleanup():
    final = INO[INO.index("bool finalizeCarouselBankTransaction"):INO.index("bool saveCarouselSlotMeta")]
    assert final.index("LittleFS.remove(CAROUSEL_BANK_TXN_FILE)") < final.index("cleanupCarouselBankBackupFiles()")
    assert "Manifest removal is the publication commit point" in final


def test_boot_recovers_interrupted_bank_before_loading_active_metadata():
    a = INO.index("void loadCarousel() {")
    load = INO[a:INO.index("void stopCarouselPlayback()", a)]
    recovery = load.index('rollbackCarouselBankTransaction("boot")')
    read_order = load.index('carouselPrefs.getUChar("count",0)')
    assert recovery < read_order


def test_02_01_filesystem_setup_is_deferred_off_nimble_host():
    h = INO.index("bool handleCarouselCommand(const uint8_t *data, size_t len) {")
    handler = INO[h:INO.index("// ======================================================\n// PRESET / DEFAULT", h)]
    assert "queueDeferredCarouselBankSetup(data+5,count,viewIntent)" in handler
    assert "beginCarouselBankTransaction" not in handler
    assert "clearCarouselSlot" not in handler
    p = INO.index("void processDeferredCarouselBankSetup() {")
    process = INO[p:h]
    assert "beginCarouselBankTransaction(touchedMask)" in process
    assert "clearCarouselSlot(order[i])" in process
    assert "sendCommandAck(0x02,0x01)" in process
    loop = INO[INO.index("void loop(){"):]
    assert loop.index("processDeferredAssetCommit();") < loop.index("processDeferredCarouselBankSetup();") < loop.index("expireStalledTransfers(now);")


def test_disconnect_can_drop_unpublished_setup_without_filesystem_work():
    a = INO.index("void handleBleDisconnected()")
    disconnect = INO[a:INO.index("#if IDOTMATRIX_USE_NIMBLE", a)]
    assert "deferredCarouselBankSetup.active" in disconnect
    assert "DeferredCarouselBankSetupState()" in disconnect
    assert "rollbackCarouselBankTransaction" not in disconnect


def test_protected_bank_rejects_undeclared_slot_media():
    a = INO.index("bool processBulkPacket")
    bulk = INO[a:INO.index("void processFA02Packet", a)]
    assert "carouselBankTxnManifest.touchedMask&(1U<<imageIndex)" in bulk
    assert 'state=reject reason=undeclared_slot' in bulk


def test_device_reset_removes_journal_and_old_bank_backups():
    assert "LittleFS.remove(CAROUSEL_BANK_TXN_FILE);" in INO
    assert "LittleFS.remove(CAROUSEL_BANK_TXN_TMP_FILE);" in INO
    assert "LittleFS.remove(carouselBankBackupFileName(i,1));" in INO
    assert "LittleFS.remove(carouselBankBackupFileName(i,3));" in INO
