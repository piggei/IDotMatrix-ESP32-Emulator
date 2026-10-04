from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _between(start: str, end: str) -> str:
    a = INO.index(start)
    b = INO.index(end, a)
    return INO[a:b]


def test_stages_preset_media_under_candidate_names_until_activation():
    assert '".new.txt" : ".new.gif"' in INO
    assert '".old.txt" : ".old.gif"' in INO
    assert "PresetSlotMeta presetStaging[PRESET_SLOT_COUNT]" in INO
    assert "uint16_t presetStagedMask" in INO
    commit = _between("bool commitPresetSlot", "bool applyPresetOrderAndStart")
    assert "if(presetBankTxnActive)" in commit
    assert "presetStagedFileName(localSlot,dataType)" in commit
    assert "LittleFS.rename(tmp,staged)" in commit
    candidate_branch = commit[commit.index("if(presetBankTxnActive)"):commit.index("// Compatibility fallback")]
    assert "presetSlots[localSlot]" not in candidate_branch
    assert "presetStaging[localSlot]=m" in candidate_branch


def test_first_preset_bulk_opens_transaction_without_erasing_authoritative_bank():
    bulk = _between("bool processBulkPacket", "void processFA02Packet")
    preset = bulk[bulk.index("if((type==1 || type==3) && presetTraceSlot)"):]
    assert "if(!presetBankTxnActive) beginPresetBankTransaction(previousPresetActive ? 1U : 0U,previousPresetSlot);" in preset
    begin = _between("void beginPresetBankTransaction(uint8_t previousActive, int8_t previousSlot)", "void requestPresetBankCleanup")
    assert "LittleFS.remove" not in begin
    assert "LittleFS.rename" not in begin
    assert "presetStagedMask=0" in begin


def test_06_02_activation_is_deferred_off_nimble_host():
    handler = _between("bool handlePresetCommand", "// ======================================================\n// BULK")
    assert "queueDeferredPresetActivation(localOrder,count)" in handler
    assert "commitPresetBankActivation" not in handler
    assert "startPresetSlot" not in handler
    process = _between("void processDeferredPresetActivation()", "void processPresetBankCleanup()")
    assert "commitPresetBankActivation(order,count)" in process
    assert "sendCommandAck(0x06,0x02)" in process


def test_activation_requires_every_declared_candidate_before_backup_or_publication():
    commit = _between("bool commitPresetBankActivation", "bool queueDeferredPresetActivation")
    validation = commit.index("fileMatchesMedia(presetStagedFileName(slot,m.dataType),m.mediaSize,m.mediaCRC)")
    backup = commit.index("LittleFS.rename(gif,gifBak)")
    publish = commit.index("LittleFS.rename(staged,dst)")
    metadata = commit.index("presetSlots[order[i]]=presetStaging[order[i]]")
    assert validation < backup < publish < metadata
    assert 'state=reject reason=incomplete' in commit


def test_publication_uses_rename_only_old_file_backups_and_rolls_back_before_metadata_switch():
    commit = _between("bool commitPresetBankActivation", "bool queueDeferredPresetActivation")
    assert "presetBackupFileName(slot,1)" in commit
    assert "presetBackupFileName(slot,3)" in commit
    assert "rollbackPresetBankPublication(order,count,backedMask,hadGifMask,hadTxtMask)" in commit
    rollback = _between("bool rollbackPresetBankPublication", "bool commitPresetBankActivation")
    assert "LittleFS.rename(gifBak,gif)" in rollback
    assert "LittleFS.rename(txtBak,txt)" in rollback


def test_abort_can_resume_the_previous_authoritative_preset():
    abort = _between("void abortPresetBankTransaction", "void clearPresetSlot")
    assert "presetBankPreviousActive" in INO
    assert "presetBankPreviousActiveSlot" in INO
    assert "startPresetSlot((uint8_t)previousSlot)" in abort
    assert "nextPresetSlot(-1)" in abort
    assert 'Serial.print(" resumed=")' in abort


def test_disconnect_and_partial_bulk_abort_only_request_loop_cleanup():
    disconnect = _between("void handleBleDisconnected()", "#if IDOTMATRIX_USE_NIMBLE")
    assert "requestPresetBankCleanup(1)" in disconnect
    assert "abortPresetBankTransaction" not in disconnect
    reset = _between("void resetBulkTransfer", "bool queueDeferredAssetCommit")
    assert "abortedPresetTransfer" in reset
    assert "requestPresetBankCleanup(2)" in reset
    cleanup = _between("void processPresetBankCleanup()", "int8_t nextPresetSlot")
    assert "abortPresetBankTransaction" in cleanup


def test_loop_orders_asset_stage_then_activation_then_cleanup():
    loop = INO[INO.index("void loop(){"):]
    asset = loop.index("processDeferredAssetCommit();")
    activation = loop.index("processDeferredPresetActivation();")
    cleanup = loop.index("processPresetBankCleanup();")
    timeout = loop.index("expireStalledTransfers(now);")
    assert asset < activation < cleanup < timeout


def test_preset_transaction_is_still_volatile_across_reboot_and_reset():
    setup = INO[INO.index("void setup(){"):INO.index("void loop(){")]
    assert "clearPresetBank(true); // Preset/Default slots are intentionally volatile across reboot." in setup
    clear_slot = _between("void clearPresetSlot", "void clearPresetBank")
    assert "presetStagedFileName(localSlot,1)" in clear_slot
    assert "presetStagedFileName(localSlot,3)" in clear_slot
    assert "presetBackupFileName(localSlot,1)" in clear_slot
    assert "presetBackupFileName(localSlot,3)" in clear_slot


def test_custom_helper_signatures_remain_arduino_autoprototype_safe():
    assert "bool queueDeferredPresetActivation(const uint8_t *order, uint8_t count)" in INO
    assert "bool commitPresetBankActivation(const uint8_t *order, uint8_t count)" in INO
    assert "bool rollbackPresetBankPublication(const uint8_t *order, uint8_t count," in INO
    signature = INO[INO.index("bool queueDeferredPresetActivation"):].splitlines()[0]
    assert "DeferredPresetActivationState" not in signature
