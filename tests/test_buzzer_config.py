#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src'
INO = (SRC / 'IDotMatrix.ino').read_text()
PIO = (ROOT / 'platformio.ini').read_text()
USER_EXAMPLE = (SRC / 'IDotMatrixUserConfig.example.h').read_text()


def preprocess(extra_flags=()):
    code = '#include "IDotMatrixHardwareConfig.h"\n'
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / 'probe.cpp'
        src.write_text(code)
        cmd = ['g++', '-E', '-dM', '-x', 'c++', f'-I{SRC}', *extra_flags, str(src)]
        out = subprocess.check_output(cmd, text=True)
    macros = {}
    for line in out.splitlines():
        if not line.startswith('#define '):
            continue
        parts = line.split(None, 2)
        if len(parts) == 3:
            macros[parts[1]] = parts[2]
    return macros


def test_c3_profile_defaults():
    required = [
        '-DIDOTMATRIX_DEFAULT_BUZZER_TYPE=2',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PIN=3',
        '-DIDOTMATRIX_DEFAULT_BUZZER_FREQUENCY_HZ=2000',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PASSIVE_TRIGGER_LOW=1',
        '-DIDOTMATRIX_DEFAULT_ALARM_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_COUNTDOWN_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_SCHEDULE_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_CONNECTION_BUZZER_ENABLED=1',
    ]
    for item in required:
        assert item in PIO, item


def test_default_profile_resolution():
    macros = preprocess([
        '-DIDOTMATRIX_DEFAULT_BUZZER_TYPE=2',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PIN=3',
        '-DIDOTMATRIX_DEFAULT_BUZZER_FREQUENCY_HZ=2000',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PASSIVE_TRIGGER_LOW=1',
        '-DIDOTMATRIX_DEFAULT_ALARM_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_COUNTDOWN_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_SCHEDULE_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_CONNECTION_BUZZER_ENABLED=1',
    ])
    assert macros['IDOTMATRIX_BUZZER_TYPE'] == 'IDOTMATRIX_DEFAULT_BUZZER_TYPE'
    assert macros['IDOTMATRIX_BUZZER_PIN'] == 'IDOTMATRIX_DEFAULT_BUZZER_PIN'
    assert macros['IDOTMATRIX_BUZZER_FREQUENCY_HZ'] == 'IDOTMATRIX_DEFAULT_BUZZER_FREQUENCY_HZ'
    assert macros['IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW'] == 'IDOTMATRIX_DEFAULT_BUZZER_PASSIVE_TRIGGER_LOW'
    assert macros['IDOTMATRIX_BUZZER_AVAILABLE'] == '1'


def test_explicit_active_override_keeps_profile_pin():
    macros = preprocess([
        '-DIDOTMATRIX_BUZZER_TYPE=1',
        '-DIDOTMATRIX_DEFAULT_BUZZER_TYPE=2',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PIN=3',
        '-DIDOTMATRIX_DEFAULT_ALARM_BUZZER_ENABLED=1',
    ])
    assert macros['IDOTMATRIX_BUZZER_TYPE'] == '1'
    assert macros['IDOTMATRIX_BUZZER_PIN'] == 'IDOTMATRIX_DEFAULT_BUZZER_PIN'
    assert macros['IDOTMATRIX_BUZZER_AVAILABLE'] == '1'
    assert macros['IDOTMATRIX_ALARM_BUZZER_ENABLED'] == 'IDOTMATRIX_DEFAULT_ALARM_BUZZER_ENABLED'



def test_explicit_passive_trigger_override_beats_profile_default():
    macros = preprocess([
        '-DIDOTMATRIX_BUZZER_TYPE=2',
        '-DIDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW=0',
        '-DIDOTMATRIX_DEFAULT_BUZZER_TYPE=2',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PASSIVE_TRIGGER_LOW=1',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PIN=3',
    ])
    assert macros['IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW'] == '0'


def test_explicit_none_disables_profile_event_defaults():
    macros = preprocess([
        '-DIDOTMATRIX_BUZZER_TYPE=0',
        '-DIDOTMATRIX_DEFAULT_BUZZER_TYPE=2',
        '-DIDOTMATRIX_DEFAULT_BUZZER_PIN=3',
        '-DIDOTMATRIX_DEFAULT_ALARM_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_COUNTDOWN_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_SCHEDULE_BUZZER_ENABLED=1',
        '-DIDOTMATRIX_DEFAULT_CONNECTION_BUZZER_ENABLED=1',
    ])
    assert macros['IDOTMATRIX_BUZZER_AVAILABLE'] == '0'
    for name in ('ALARM','COUNTDOWN','SCHEDULE','CONNECTION'):
        assert macros[f'IDOTMATRIX_{name}_BUZZER_ENABLED'] == '0'


def test_source_uses_ledc_for_passive_output():
    assert '#include <esp32-hal-ledc.h>' in INO
    assert 'ledcAttach(IDOTMATRIX_BUZZER_PIN, IDOTMATRIX_BUZZER_FREQUENCY_HZ, BUZZER_LEDC_RESOLUTION_BITS)' in INO
    assert 'ledcWriteTone(IDOTMATRIX_BUZZER_PIN, IDOTMATRIX_BUZZER_FREQUENCY_HZ)' in INO
    assert 'IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW ? BUZZER_LEDC_MAX_DUTY : 0u' in INO
    assert 'digitalWrite(IDOTMATRIX_BUZZER_PIN, IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW ? HIGH : LOW)' in INO
    assert 'digitalWrite(IDOTMATRIX_BUZZER_PIN' in INO


def test_user_template_exposes_buzzer_settings():
    for token in (
        'IDOTMATRIX_BUZZER_TYPE', 'IDOTMATRIX_BUZZER_PIN',
        'IDOTMATRIX_BUZZER_FREQUENCY_HZ', 'IDOTMATRIX_BUZZER_PASSIVE_TRIGGER_LOW',
        'IDOTMATRIX_ALARM_BUZZER_ENABLED',
        'IDOTMATRIX_COUNTDOWN_BUZZER_ENABLED', 'IDOTMATRIX_SCHEDULE_BUZZER_ENABLED',
        'IDOTMATRIX_CONNECTION_BUZZER_ENABLED'):
        assert token in USER_EXAMPLE


if __name__ == '__main__':
    test_c3_profile_defaults()
    test_default_profile_resolution()
    test_explicit_active_override_keeps_profile_pin()
    test_explicit_passive_trigger_override_beats_profile_default()
    test_explicit_none_disables_profile_event_defaults()
    test_source_uses_ledc_for_passive_output()
    test_user_template_exposes_buzzer_settings()
    print('buzzer config tests: PASS')
