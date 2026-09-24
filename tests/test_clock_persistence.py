from pathlib import Path

src = Path('src/IDotMatrix.ino').read_text()

assert '#define CLOCK_NVS_NAMESPACE      "idotmatrix"' in src
for key in ['cvalid', 'cstyle', 'cflags', 'cr', 'cg', 'cb']:
    assert f'"{key}"' in src

assert 'void loadClockPreferencesFromNVS()' in src
assert 'void scheduleClockPreferencesSave()' in src
assert 'void flushClockPreferencesSaveIfNeeded()' in src
assert 'scheduleClockPreferencesSave();' in src
assert 'flushClockPreferencesSaveIfNeeded();' in src

load_pos = src.index('loadClockPreferencesFromNVS();')
boot_pos = src.index('  applyBootDisplayPolicy();', load_pos)
assert load_pos < boot_pos, 'Clock presentation preferences must load before RTC boot-to-Clock rendering'

# The persisted state must cover every presentation property carried by 06/01.
for token in ['clockStyle', 'clock24h', 'clockShowDate', 'clockColor.r', 'clockColor.g', 'clockColor.b']:
    assert token in src[src.index('void flushClockPreferencesSaveIfNeeded()'):src.index('void loadClockPreferencesFromNVS()')]

# Device reset clears the shared idotmatrix namespace and restores renderer defaults.
reset_anchor = src.index('DEVICE RESET: persistent emulator state cleared')
reset_window = src[max(0, reset_anchor-1500):reset_anchor]
assert 'bp.clear()' in reset_window
assert 'clock24h=false' in reset_window
assert 'clockColor=CRGB::White' in reset_window

print('Clock presentation persistence regression checks passed.')
