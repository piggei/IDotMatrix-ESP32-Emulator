from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UPDATER = (ROOT / "update_idotmatrix_emulator.sh").read_text()
RUNNER = (ROOT / "tests" / "run_tests.py").read_text()


def test_update_helper_uses_dependency_free_runner():
    assert 'tests/run_tests.py' in UPDATER
    assert 'python3 -m pytest' not in UPDATER
    assert '-m pytest' not in UPDATER
    assert 'TEST_PYTHON' in UPDATER


def test_update_helper_defaults_to_waveshare_target():
    assert 'PIO_ENV="${PIO_ENV:-waveshare_s3_rgbmatrix_64x64}"' in UPDATER
    assert 'Waveshare ESP32-S3 RGB Matrix' in UPDATER
    assert 'usb-Espressif_USB_JTAG_serial_debug_unit_' in UPDATER
    assert '[[ "$PIO_ENV" == waveshare_s3_rgbmatrix_* ]]' in UPDATER


def test_dependency_free_runner_has_no_pytest_import():
    assert 'import pytest' not in RUNNER
    assert 'from pytest' not in RUNNER
    assert 'test_*.py' in RUNNER
    assert 'os.chdir(repository_root)' in RUNNER
