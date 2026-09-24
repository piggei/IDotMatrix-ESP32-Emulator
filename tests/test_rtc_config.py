#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src'
CFG = (SRC / 'IDotMatrixHardwareConfig.h').read_text()
INO = (SRC / 'IDotMatrix.ino').read_text()
RTC_CPP = (SRC / 'IDotMatrixRtc.cpp').read_text()
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


def test_c3_rtc_and_shared_bus_defaults():
    for token in (
        '-DIDOTMATRIX_DEFAULT_I2C_SDA_PIN=1',
        '-DIDOTMATRIX_DEFAULT_I2C_SCL_PIN=2',
        '-DIDOTMATRIX_DEFAULT_RTC_TYPE=1',
        '-DIDOTMATRIX_DEFAULT_RTC_I2C_ADDRESS=0x68',
        '-DIDOTMATRIX_DEFAULT_RTC_SYNC_FROM_BLE=1',
        '-DIDOTMATRIX_DEFAULT_RTC_RETRY_INTERVAL_MS=60000UL',
    ):
        assert token in PIO, token


def test_default_rtc_resolution():
    macros = preprocess([
        '-DIDOTMATRIX_DEFAULT_I2C_SDA_PIN=1',
        '-DIDOTMATRIX_DEFAULT_I2C_SCL_PIN=2',
        '-DIDOTMATRIX_DEFAULT_RTC_TYPE=1',
        '-DIDOTMATRIX_DEFAULT_RTC_I2C_ADDRESS=0x68',
        '-DIDOTMATRIX_DEFAULT_RTC_SYNC_FROM_BLE=1',
        '-DIDOTMATRIX_DEFAULT_RTC_RETRY_INTERVAL_MS=60000UL',
    ])
    assert macros['IDOTMATRIX_I2C_SDA_PIN'] == 'IDOTMATRIX_DEFAULT_I2C_SDA_PIN'
    assert macros['IDOTMATRIX_I2C_SCL_PIN'] == 'IDOTMATRIX_DEFAULT_I2C_SCL_PIN'
    assert macros['IDOTMATRIX_RTC_TYPE'] == 'IDOTMATRIX_DEFAULT_RTC_TYPE'
    assert macros['IDOTMATRIX_RTC_I2C_ADDRESS'] == 'IDOTMATRIX_DEFAULT_RTC_I2C_ADDRESS'
    assert macros['IDOTMATRIX_RTC_AVAILABLE'] == '1'
    assert macros['IDOTMATRIX_RTC_RETRY_INTERVAL_MS'] == 'IDOTMATRIX_DEFAULT_RTC_RETRY_INTERVAL_MS'


def test_local_none_disables_profile_rtc():
    macros = preprocess([
        '-DIDOTMATRIX_RTC_TYPE=0',
        '-DIDOTMATRIX_DEFAULT_RTC_TYPE=1',
        '-DIDOTMATRIX_DEFAULT_RTC_I2C_ADDRESS=0x68',
    ])
    assert macros['IDOTMATRIX_RTC_TYPE'] == '0'
    assert macros['IDOTMATRIX_RTC_AVAILABLE'] == '0'


def test_explicit_ds3231_mpu_0x68_conflict_is_rejected():
    with tempfile.TemporaryDirectory() as td:
        probe = Path(td) / 'probe.cpp'
        probe.write_text('#include "IDotMatrixHardwareConfig.h"\n')
        result = subprocess.run([
            'g++', '-E', f'-I{SRC}',
            '-DIDOTMATRIX_RTC_TYPE=1',
            '-DIDOTMATRIX_ACCEL_DRIVER_ICM20689',
            '-DIDOTMATRIX_ACCEL_I2C_ADDRESS=0x68',
            str(probe),
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert result.returncode != 0


def test_mpu_auto_probe_remains_allowed_with_ds3231():
    macros = preprocess([
        '-DIDOTMATRIX_RTC_TYPE=1',
        '-DIDOTMATRIX_ACCEL_DRIVER_ICM20689',
        '-DIDOTMATRIX_ACCEL_I2C_ADDRESS=0',
    ])
    assert macros['IDOTMATRIX_RTC_AVAILABLE'] == '1'
    assert macros['IDOTMATRIX_ACCEL_I2C_ADDRESS'] == '0'


def test_rtc_is_direct_i2c_and_does_not_reinitialize_wire():
    assert 'RTClib' not in RTC_CPP
    assert 'Wire.begin(' not in RTC_CPP
    assert 'writeRegisters(DS3231_REG_SECONDS' in RTC_CPP
    assert 'DS3231_STATUS_OSF' in RTC_CPP
    assert 'rtc.begin(Wire, IDOTMATRIX_RTC_I2C_ADDRESS)' in INO
    assert 'rtc.adjust(rtcValue)' in INO


def test_user_template_exposes_rtc_settings():
    for token in (
        'IDOTMATRIX_RTC_TYPE', 'IDOTMATRIX_RTC_DS3231',
        'IDOTMATRIX_RTC_I2C_ADDRESS', 'IDOTMATRIX_RTC_SYNC_FROM_BLE',
        'IDOTMATRIX_RTC_RETRY_INTERVAL_MS', 'IDOTMATRIX_RTC_DIAGNOSTICS'):
        assert token in USER_EXAMPLE



def test_rtc_runtime_retry_and_recovery_policy():
    assert 'uint32_t rtcRetryNextAt = 0;' in INO
    assert 'updateRtcRecovery(now);' in INO
    assert 'IDOTMATRIX_RTC_RETRY_INTERVAL_MS' in INO
    assert 'rtcRetryNextAt=millis();' in INO  # valid BLE time sync requests an immediate loop-side retry
    assert 'RTC: recovered on I2C address 0x' in INO
    assert 'RTC: recovered and synchronized from software time' in INO
    assert 'RTC: software clock initialized from recovered RTC' in INO
    assert 'syncYear=bootRtc.year' in INO


def test_rtc_retry_interval_rejects_unsafe_values():
    with tempfile.TemporaryDirectory() as td:
        probe = Path(td) / 'probe.cpp'
        probe.write_text('#include "IDotMatrixHardwareConfig.h"\n')
        result = subprocess.run([
            'g++', '-E', f'-I{SRC}',
            '-DIDOTMATRIX_RTC_TYPE=1',
            '-DIDOTMATRIX_RTC_RETRY_INTERVAL_MS=999',
            str(probe),
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert result.returncode != 0

def test_rtc_driver_syntax_with_minimal_arduino_wire_stubs():
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        (td / 'Arduino.h').write_text(r'''#pragma once
#include <cstddef>
#include <cstdint>
using std::size_t;
void delay(unsigned long);
''')
        (td / 'Wire.h').write_text(r'''#pragma once
#include <cstddef>
#include <cstdint>
using std::size_t;
class TwoWire {
public:
  void beginTransmission(uint8_t);
  size_t write(uint8_t);
  uint8_t endTransmission(bool = true);
  size_t requestFrom(uint8_t, uint8_t, bool = true);
  int available();
  int read();
};
extern TwoWire Wire;
''')
        subprocess.check_call([
            'g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
            f'-I{td}', f'-I{SRC}', str(SRC / 'IDotMatrixRtc.cpp')
        ])


if __name__ == '__main__':
    test_c3_rtc_and_shared_bus_defaults()
    test_default_rtc_resolution()
    test_local_none_disables_profile_rtc()
    test_explicit_ds3231_mpu_0x68_conflict_is_rejected()
    test_mpu_auto_probe_remains_allowed_with_ds3231()
    test_rtc_is_direct_i2c_and_does_not_reinitialize_wire()
    test_user_template_exposes_rtc_settings()
    test_rtc_runtime_retry_and_recovery_policy()
    test_rtc_retry_interval_rejects_unsafe_values()
    test_rtc_driver_syntax_with_minimal_arduino_wire_stubs()
    print('RTC config/driver tests: PASS')
