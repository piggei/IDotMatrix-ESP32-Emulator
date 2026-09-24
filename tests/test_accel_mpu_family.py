from pathlib import Path

root = Path(__file__).resolve().parents[1]
driver = (root / 'src/drivers/IDotMatrixAccelMPUFamily.cpp').read_text()
config = (root / 'src/IDotMatrixHardwareConfig.h').read_text()
common = (root / 'src/IDotMatrixAccelerometer.cpp').read_text()
pio = (root / 'platformio.ini').read_text()

required_driver_tokens = {
    'ICM-20689 WHO_AM_I': 'kWhoAmIICM20689 = 0x98',
    'MPU-6050 WHO_AM_I primary': 'kWhoAmIMPU6050Primary = 0x68',
    'MPU-6050 WHO_AM_I secondary': 'kWhoAmIMPU6050Secondary = 0x69',
    'ACCEL_CONFIG2 register': 'kRegAccelConfig2 = 0x1D',
    'accelerometer output register': 'kRegAccelXoutH = 0x3B',
    '+/-2g scale': '1.0f / 16384.0f',
    'ICM DLPF configuration': 'sensorWhoAmI == kWhoAmIICM20689 && !writeRegister(kRegAccelConfig2, 0x03)',
    'ICM DLPF readback': 'sensorWhoAmI == kWhoAmIICM20689 && !verifyRegisterMasked(kRegAccelConfig2, 0x0F, 0x03)',
}
for label, token in required_driver_tokens.items():
    assert token in driver, f'missing {label}'

assert 'IDOTMATRIX_ACCEL_DRIVER_ICM20689' in config
assert 'IDOTMATRIX_ACCEL_DRIVER_MPU6050' in config
assert 'IDOTMATRIX_ACCEL_DRIVER_ICM20689' in common
assert 'IDotMatrixAccelMPUFamily.h' in common
assert '[env:matrixportal_s3_hub75_64_icm20689]' in pio
assert '-DIDOTMATRIX_DEFAULT_ACCEL_DRIVER_ICM20689' in pio
assert '-DIDOTMATRIX_DEFAULT_ACCEL_I2C_ADDRESS=0' in pio

print('MPU-family / ICM-20689 backend structure: PASS')

assert 'ACCEL PROBE 0x' in driver
assert 'ACK=' in driver
assert 'WHO_AM_I=' in driver
assert 'ACCEL RESULT: ready=' in driver
print('MPU-family qualification diagnostics: PASS')

config_text = (root / 'src/IDotMatrixHardwareConfig.h').read_text()
assert '#define IDOTMATRIX_ORIENTATION_DIAGNOSTICS 0' in config_text
assert '-DIDOTMATRIX_ORIENTATION_DIAGNOSTICS=1' not in pio
assert '-DIDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS=1' not in pio
orientation = (root / 'src/IDotMatrixOrientation.cpp').read_text()
assert 'ORIENTATION SENSOR ERROR: driver=' in orientation
print('post-qualification diagnostics cleanup: PASS')
assert '#if IDOTMATRIX_ORIENTATION_SAMPLE_DIAGNOSTICS\nuint32_t lastDiagAt = 0;' in orientation
print('sample-diagnostic state cleanup: PASS')
