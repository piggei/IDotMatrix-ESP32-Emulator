from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


def test_memory_telemetry_translation_unit_syntax_with_minimal_stubs():
    compiler = shutil.which("g++") or shutil.which("c++")
    assert compiler, "host C++ compiler not available"

    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        (td / "Arduino.h").write_text(r'''
#pragma once
#include <cstddef>
#include <cstdint>
struct SerialClass {
  template<class T> void print(const T&) {}
  template<class T> void println(const T&) {}
};
extern SerialClass Serial;
struct ESPClass { std::size_t getPsramSize() const { return 16u * 1024u * 1024u; } };
extern ESPClass ESP;
inline uint32_t millis() { return 0; }
''')
        (td / "esp_heap_caps.h").write_text(r'''
#pragma once
#include <cstddef>
#include <cstdint>
#define MALLOC_CAP_INTERNAL 0x01u
#define MALLOC_CAP_8BIT     0x02u
#define MALLOC_CAP_DMA      0x04u
#define MALLOC_CAP_SPIRAM   0x08u
inline std::size_t heap_caps_get_free_size(uint32_t) { return 1; }
inline std::size_t heap_caps_get_minimum_free_size(uint32_t) { return 1; }
inline std::size_t heap_caps_get_largest_free_block(uint32_t) { return 1; }
''')
        (td / "globals.cpp").write_text(r'''
#include "Arduino.h"
SerialClass Serial;
ESPClass ESP;
''')
        cmd = [
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
            f"-I{td}", f"-I{SRC}",
            "-DIDOTMATRIX_MEMORY_TELEMETRY=1", "-DBOARD_HAS_PSRAM=1",
            str(SRC / "IDotMatrixMemoryTelemetry.cpp"), str(td / "globals.cpp"),
        ]
        subprocess.run(cmd, check=True)
