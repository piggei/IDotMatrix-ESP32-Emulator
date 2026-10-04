from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INO_PATH = ROOT / "src" / "IDotMatrix.ino"
INO = INO_PATH.read_text(encoding="utf-8")


def _extract():
    a = INO.index("uint8_t *gifStageData = nullptr;")
    b = INO.index("void destroyGIFDecoder()", a)
    state_and_stage = INO[a:b]
    c = INO.index("// AnimatedGIF source callbacks.", b)
    d = INO.index("\nvoid GIFDraw(", c)
    callbacks = INO[c:d]
    return state_and_stage + "\n" + callbacks


def _compile(stage_max: int):
    compiler = shutil.which("g++") or shutil.which("c++")
    assert compiler, "host C++ compiler not available"
    code = _extract()
    with tempfile.TemporaryDirectory() as td_raw:
        td = Path(td_raw)
        (td / "stub.cpp").write_text(r'''
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>
#include <cstdlib>
using std::size_t;
template<class T> T min(T a, T b) { return std::min(a,b); }
class String {
public:
  std::string s;
  String() = default;
  String(const char *v): s(v ? v : "") {}
  String &operator=(const char *v) { s = v ? v : ""; return *this; }
  bool operator==(const char *v) const { return s == (v ? v : ""); }
};
class File {
public:
  bool good = true;
  size_t sz = 1024;
  size_t pos = 0;
  File() = default;
  explicit operator bool() const { return good; }
  size_t size() const { return sz; }
  void close() { good=false; }
  int read(uint8_t *p, size_t n) { if(!good) return -1; std::memset(p,0,n); pos+=n; return (int)n; }
  bool seek(uint32_t p, int) { pos=p; return true; }
  size_t position() const { return pos; }
};
struct LittleFSClass { File open(const char*, const char*) { return File(); } };
LittleFSClass LittleFS;
bool littleFsReady=true;
struct SerialClass {
  template<class T> void print(const T&) {}
  template<class T> void println(const T&) {}
};
SerialClass Serial;
#define DEBUG_SERIAL 1
#define IDOTMATRIX_MEMORY_TELEMETRY 1
#define MALLOC_CAP_SPIRAM 0x01u
#define MALLOC_CAP_8BIT 0x02u
#define SeekSet 0
inline size_t heap_caps_get_free_size(uint32_t){ return 16u*1024u*1024u; }
inline size_t heap_caps_get_largest_free_block(uint32_t){ return 15u*1024u*1024u; }
inline void *heap_caps_malloc(size_t n,uint32_t){ return std::malloc(n); }
inline void heap_caps_free(void *p){ std::free(p); }
inline uint32_t micros(){ return 0; }
inline void yield(){}
inline void idotMemoryTelemetrySnapshot(const char*){}
inline void idotMemoryTelemetryLatency(const char*,uint32_t){}
struct GIFFILE { void *fHandle=nullptr; int32_t iSize=0; int32_t iPos=0; };
''' + f"\n#define IDOTMATRIX_GIF_PSRAM_STAGE_MAX_BYTES {stage_max}UL\n#define IDOTMATRIX_GIF_PSRAM_RESERVE_BYTES 4194304UL\n" + code + "\nint main(){ return 0; }\n", encoding="utf-8")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsyntax-only",str(td/"stub.cpp")], check=True)


def test_staging_and_callbacks_compile_with_psram_enabled():
    _compile(2097152)


def test_staging_and_callbacks_compile_with_staging_disabled():
    _compile(0)
