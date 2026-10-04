from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _extract():
    a = INO.index("void reportCarouselGifPrefetch(")
    b = INO.index("void destroyGIFDecoder()", a)
    return INO[a:b]


def _compile(enabled: int):
    compiler = shutil.which("g++") or shutil.which("c++")
    assert compiler, "host C++ compiler not available"
    code = _extract()
    stub = r'''
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
  const char *c_str() const { return s.c_str(); }
};
class File {
public:
  bool good=false;
  size_t sz=8192;
  File() = default;
  explicit operator bool() const { return good; }
  size_t size() const { return sz; }
  void close(){ good=false; }
  int read(uint8_t *p,size_t n){ if(!good) return -1; std::memset(p,0,n); return (int)n; }
};
struct LittleFSClass {
  bool exists(const char*) const { return true; }
  File open(const char*,const char*) { File f; f.good=true; return f; }
} LittleFS;
struct SerialClass {
  template<class T> void print(const T&) {}
  template<class T, class U> void print(const T&, const U&) {}
  template<class T> void println(const T&) {}
} Serial;
#define DEBUG_SERIAL 1
#define IDOTMATRIX_MEMORY_TELEMETRY 1
#define MALLOC_CAP_SPIRAM 0x01u
#define MALLOC_CAP_8BIT 0x02u
#define CAROUSEL_SLOT_COUNT 12
#define IDOTMATRIX_GIF_PSRAM_CACHE_ENTRY_MAX_BYTES 524288UL
#define IDOTMATRIX_GIF_PSRAM_RESERVE_BYTES 4194304UL
#define IDOTMATRIX_CAROUSEL_GIF_PREFETCH_DELAY_MS 250UL
#define IDOTMATRIX_CAROUSEL_GIF_PREFETCH_CHUNK_BYTES 4096U
#define GIF_CACHE_PATH_MAX 48U
inline size_t heap_caps_get_free_size(uint32_t){ return 16u*1024u*1024u; }
inline size_t heap_caps_get_largest_free_block(uint32_t){ return 15u*1024u*1024u; }
inline void *heap_caps_malloc(size_t n,uint32_t){ return std::malloc(n); }
inline void heap_caps_free(void *p){ std::free(p); }
inline uint32_t millis(){ return 1000; }
inline uint32_t micros(){ return 1000; }
inline void idotMemoryTelemetrySnapshot(const char*){}
inline void idotMemoryTelemetryLatency(const char*,uint32_t){}
struct CarouselSlotMeta { bool configured=false; uint8_t dataType=1; uint16_t dwellSeconds=5; uint32_t mediaSize=8192; uint32_t mediaCRC=1; };
CarouselSlotMeta carouselSlots[CAROUSEL_SLOT_COUNT];
bool littleFsReady=true;
bool carouselActive=true;
int8_t carouselActiveSlot=0;
bool gifCarouselPlaybackFileActive=false;
int8_t gifCacheActiveIndex=-1;
uint8_t *gifStageData=nullptr;
struct CarouselGifPrefetchState {
  bool scheduled=false; bool active=false; uint8_t slot=0; uint32_t readyAt=0;
  uint8_t *data=nullptr; size_t size=0; size_t copied=0; uint32_t expectedCRC=0;
  uint32_t runningCRC=0xFFFFFFFFUL; uint32_t startedAtUs=0; File file; char path[GIF_CACHE_PATH_MAX]={0};
};
CarouselGifPrefetchState carouselGifPrefetch;
uint32_t carouselGifPrefetchAttempts=0, carouselGifPrefetchOk=0, carouselGifPrefetchSkips=0;
uint32_t carouselGifPrefetchFailures=0, carouselGifPrefetchCancels=0;
inline bool gifCacheConfigured(){ return true; }
inline int8_t nextCarouselSlot(int8_t){ return 1; }
inline String carouselFileName(uint8_t,const uint8_t){ return String("/car1.gif"); }
inline bool gifCacheMatches(const char*,uint32_t,uint32_t){ return false; }
inline bool gifCacheInsertOwnedBuffer(const char*,size_t,uint32_t,uint8_t*,const char*,int8_t *slot){ if(slot)*slot=0; return true; }
inline uint32_t crc32Update(uint32_t crc,const uint8_t*,size_t){ return crc; }
'''
    with tempfile.TemporaryDirectory() as td_raw:
        td=Path(td_raw)
        (td/"stub.cpp").write_text(stub + f"\n#define IDOTMATRIX_CAROUSEL_GIF_PREFETCH {enabled}\n" + code + "\nint main(){ return 0; }\n", encoding="utf-8")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsyntax-only",str(td/"stub.cpp")],check=True)


def test_prefetch_block_compiles_when_enabled():
    _compile(1)


def test_prefetch_block_compiles_when_disabled():
    _compile(0)
