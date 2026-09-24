from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PIO = (ROOT / 'platformio.ini').read_text()
INO = (ROOT / 'src' / 'IDotMatrix.ino').read_text()
README = (ROOT / 'README.md').read_text()

def test_c3_native_usb_serial_flags_present():
    section = PIO.split('[env:esp32c3_ws2812_16]',1)[1]
    if '\n[env:' in section:
        section = section.split('\n[env:',1)[0]
    assert '-DARDUINO_USB_MODE=1' in section
    assert '-DARDUINO_USB_CDC_ON_BOOT=1' in section

def test_single_startup_summary_covers_rtc_boot_state():
    for token in (
        '=== IDOTMATRIX STARTUP SUMMARY ===',
        'USER CONFIG:', 'I2C BUS:', 'RTC BACKEND:', 'RTC READY:',
        'RTC TIME VALID:', 'RTC STATUS:', 'RTC NOW:',
        'SOFTWARE TIME SYNCED:', 'BOOT DISPLAY MODE:', 'SCREEN ON:'
    ):
        assert token in INO, token
    assert INO.count('printStartupHardwareSummary();') == 1
    assert 'startupSummaryRepeatsRemaining' not in INO

def test_arduino_ide_native_usb_documented():
    assert 'USB CDC On Boot = Enabled' in README
    assert 'USB Mode = Hardware CDC and JTAG' in README

if __name__ == '__main__':
    test_c3_native_usb_serial_flags_present()
    test_single_startup_summary_covers_rtc_boot_state()
    test_arduino_ide_native_usb_documented()
    print('C3 serial/RTC diagnostics tests: PASS')
