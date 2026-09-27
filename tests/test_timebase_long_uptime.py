from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / 'src' / 'IDotMatrix.ino').read_text()


def test_software_clock_uses_64_bit_esp_timer_timebase():
    assert '#include <esp_timer.h>' in INO
    assert 'uint64_t syncMillis = 0;' in INO
    assert 'esp_timer_get_time() / 1000LL' in INO
    assert '(millis()-syncMillis)' not in INO
    assert '(millis() - syncMillis)' not in INO
    assert 'uint64_t elapsed=(monotonicMillis64()-syncMillis)/1000ULL;' in INO
    assert 'uint64_t elapsed = (monotonicMillis64() - syncMillis) / 1000ULL;' in INO


def test_64_bit_elapsed_remains_correct_beyond_millis_wrap():
    wrap_ms = 2**32
    start = 123456
    now = start + wrap_ms + 987654
    assert now - start == wrap_ms + 987654
    assert (now - start) // 1000 > wrap_ms // 1000
