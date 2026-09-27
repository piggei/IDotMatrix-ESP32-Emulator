from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / 'src' / 'IDotMatrix.ino').read_text()


def test_runtime_uses_host_tested_idle_policy():
    assert 'idotPassiveBuzzerIdleDuty' in INO
    assert 'idotPassiveBuzzerIdleLevel' in INO
    assert 'ledcWriteTone(IDOTMATRIX_BUZZER_PIN, IDOTMATRIX_BUZZER_FREQUENCY_HZ)' in INO


def test_buzzer_policy_compiles_and_maps_both_polarities():
    compiler = shutil.which('g++') or shutil.which('c++')
    if compiler is None:
        return
    source = r'''
#include <cassert>
#include "IDotMatrixBuzzerPolicy.h"
int main() {
  const unsigned maxDuty = 255;
  assert(idotPassiveBuzzerIdleDuty(true, maxDuty) == maxDuty);
  assert(idotPassiveBuzzerIdleLevel(true) == 1);
  assert(idotPassiveBuzzerIdleDuty(false, maxDuty) == 0);
  assert(idotPassiveBuzzerIdleLevel(false) == 0);
  return 0;
}
'''
    with tempfile.TemporaryDirectory() as td:
        cpp = Path(td) / 'buzzer.cpp'
        exe = Path(td) / 'buzzer'
        cpp.write_text(source)
        subprocess.run([compiler, '-std=c++11', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT/'src'), str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
