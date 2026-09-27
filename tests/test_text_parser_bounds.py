from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / 'src' / 'IDotMatrix.ino').read_text()
HEADER = ROOT / 'src' / 'IDotMatrixProtocolGuards.h'


def test_parser_uses_shared_minimum_length_guard():
    assert 'if(!idotTextPayloadHasMarker(data, len, TEXT_GLOBAL_HEADER)) return false;' in INO
    assert 'const uint8_t marker=data[TEXT_GLOBAL_HEADER];' in INO


def test_text_guard_rejects_lengths_zero_through_header_and_accepts_marker_byte():
    compiler = shutil.which('g++') or shutil.which('c++')
    if compiler is None:
        return
    source = r'''
#include <cassert>
#include <cstdint>
#include "IDotMatrixProtocolGuards.h"
int main() {
  uint8_t data[32] = {};
  constexpr size_t H = 14;
  assert(!idotTextPayloadHasMarker(nullptr, 15, H));
  for (size_t len = 0; len <= H; ++len) assert(!idotTextPayloadHasMarker(data, len, H));
  assert(idotTextPayloadHasMarker(data, H + 1, H));
  assert(idotTextPayloadHasMarker(data, 15 + 20, H));
  assert(idotTextPayloadHasMarker(data, 15 + 68, H));
  assert(idotTextPayloadHasMarker(data, 15 + 260, H));
  return 0;
}
'''
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / 'guard.cpp'
        exe = Path(td) / 'guard'
        cpp.write_text(source)
        subprocess.run([compiler, '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT/'src'), str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
