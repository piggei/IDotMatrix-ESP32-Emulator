from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HW = (ROOT / "src" / "IDotMatrixHardwareConfig.h").read_text(encoding="utf-8")
OTA = (ROOT / "src" / "IDotMatrixOta.cpp").read_text(encoding="utf-8")


def test_boot_button_short_reboot_long_ota():
    assert "IDOTMATRIX_OTA_TRIGGER_DEBOUNCE_MS 40UL" in HW
    assert 'BOOT BUTTON: short press -> reboot' in OTA
    assert 'ESP.restart();' in OTA
    assert 'BOOT BUTTON: long press -> OTA maintenance' in OTA
    assert 'startAccessPoint();' in OTA
    assert 'triggerLongActionDone = true' in OTA


def test_long_press_is_consumed_and_does_not_reboot_on_release():
    release_block = OTA[OTA.index('if (triggerStablePressed)'):OTA.index('if (triggerStablePressed && !triggerLongActionDone')]
    assert 'const bool shortPress = !triggerLongActionDone' in release_block
    assert 'if (shortPress)' in release_block
