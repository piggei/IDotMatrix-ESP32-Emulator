from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
config = (root / 'src/IDotMatrixHardwareConfig.h').read_text()
pio = (root / 'platformio.ini').read_text()
updater = (root / 'update_idotmatrix_emulator.sh').read_text()
gitignore = (root / '.gitignore').read_text()
example = (root / 'src/IDotMatrixUserConfig.example.h').read_text()

assert '__has_include("IDotMatrixUserConfig.h")' in config
assert '#include "IDotMatrixUserConfig.h"' in config
assert 'IDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH' in config
assert 'IDOTMATRIX_DEFAULT_ACCEL_DRIVER_ICM20689' in config
assert 'IDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS' in config
assert 'IDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION' in config

assert '-DIDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH' in pio
assert '-DIDOTMATRIX_DEFAULT_ACCEL_DRIVER_ICM20689' in pio
assert '-DIDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS=0' in pio
assert '-DIDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION=0' in pio

assert "--exclude='src/IDotMatrixUserConfig.h'" in updater
assert 'src/IDotMatrixUserConfig.h' in gitignore
assert 'IDOTMATRIX_ACCEL_DRIVER_ICM20689' in example
assert 'IDOTMATRIX_I2C_SDA_PIN' in example
assert 'IDOTMATRIX_I2C_SCL_PIN' in example
assert 'IDOTMATRIX_ORIENTATION_DIAGNOSTICS' in example
assert 'IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS' in example

# Verify that an explicit local driver/address/mount selection wins over a
# conflicting PlatformIO fallback without producing two active backends.
with tempfile.TemporaryDirectory() as tmp:
    td = Path(tmp)
    (td / 'IDotMatrixUserConfig.h').write_text(
        '#pragma once\n'
        '#define IDOTMATRIX_ACCEL_DRIVER_ICM20689\n'
        '#define IDOTMATRIX_I2C_SDA_PIN 8\n'
        '#define IDOTMATRIX_I2C_SCL_PIN 9\n'
        '#define IDOTMATRIX_ACCEL_I2C_ADDRESS 0x69\n'
        '#define IDOTMATRIX_ACCEL_MOUNT_ROTATION 90\n'
    )
    probe = td / 'probe.cpp'
    probe.write_text('#include "IDotMatrixHardwareConfig.h"\n')
    cmd = [
        'g++', '-dM', '-E',
        f'-I{td}', f'-I{root / "src"}',
        '-DIDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH',
        '-DIDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS=0x19',
        '-DIDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION=0',
        str(probe),
    ]
    macros = subprocess.check_output(cmd, text=True)

assert '#define IDOTMATRIX_ACCEL_DRIVER_ICM20689' in macros
assert '#define IDOTMATRIX_ACCEL_DRIVER_LIS3DH' not in macros
assert '#define IDOTMATRIX_ACCEL_I2C_ADDRESS 0x69' in macros
assert '#define IDOTMATRIX_ACCEL_MOUNT_ROTATION 90' in macros
assert '#define IDOTMATRIX_I2C_SDA_PIN 8' in macros
assert '#define IDOTMATRIX_I2C_SCL_PIN 9' in macros
assert '#define IDOTMATRIX_USER_CONFIG_PRESENT 1' in macros

# With no local override, the checked-in profile defaults still resolve to the
# same effective backend/address/mount values used by Build 173.
with tempfile.TemporaryDirectory() as tmp:
    td = Path(tmp)
    probe = td / 'probe.cpp'
    probe.write_text('#include "IDotMatrixHardwareConfig.h"\n')
    cmd = [
        'g++', '-dM', '-E',
        f'-I{td}', f'-I{root / "src"}',
        '-DIDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH',
        '-DIDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS=0x19',
        '-DIDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION=0',
        str(probe),
    ]
    default_macros = subprocess.check_output(cmd, text=True)

assert '#define IDOTMATRIX_ACCEL_DRIVER_LIS3DH' in default_macros
assert '#define IDOTMATRIX_ACCEL_I2C_ADDRESS IDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS' in default_macros
assert '#define IDOTMATRIX_ACCEL_MOUNT_ROTATION IDOTMATRIX_DEFAULT_ACCEL_MOUNT_ROTATION' in default_macros
assert '#define IDOTMATRIX_USER_CONFIG_PRESENT 0' in default_macros


# Switching only the backend must not inherit the previous profile's sensor
# address (e.g. ICM-20689 must not inherit the LIS3DH 0x19 fallback).
with tempfile.TemporaryDirectory() as tmp:
    td = Path(tmp)
    (td / 'IDotMatrixUserConfig.h').write_text(
        '#pragma once\n'
        '#define IDOTMATRIX_ACCEL_DRIVER_ICM20689\n'
    )
    probe = td / 'probe.cpp'
    probe.write_text('#include "IDotMatrixHardwareConfig.h"\n')
    cmd = [
        'g++', '-dM', '-E',
        f'-I{td}', f'-I{root / "src"}',
        '-DIDOTMATRIX_DEFAULT_ACCEL_DRIVER_LIS3DH',
        '-DIDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS=0x19',
        str(probe),
    ]
    driver_only_macros = subprocess.check_output(cmd, text=True)

assert '#define IDOTMATRIX_ACCEL_DRIVER_ICM20689' in driver_only_macros
assert '#define IDOTMATRIX_ACCEL_DRIVER_LIS3DH' not in driver_only_macros
assert '#define IDOTMATRIX_ACCEL_I2C_ADDRESS 0x19' not in driver_only_macros

# Invalid one-pin I2C overrides remain a compile-time error.
with tempfile.TemporaryDirectory() as tmp:
    td = Path(tmp)
    (td / 'IDotMatrixUserConfig.h').write_text(
        '#pragma once\n'
        '#define IDOTMATRIX_I2C_SDA_PIN 8\n'
    )
    probe = td / 'probe.cpp'
    probe.write_text('#include "IDotMatrixHardwareConfig.h"\n')
    result = subprocess.run(
        ['g++', '-E', f'-I{td}', f'-I{root / "src"}', str(probe)],
        text=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
assert result.returncode != 0

print('optional user hardware config precedence/preservation: PASS')
