from pathlib import Path
import shutil, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[1]
INO=(ROOT/'src'/'IDotMatrix.ino').read_text(encoding='utf-8')

def _extract():
    a=INO.index('bool loadCarouselRawFrame(')
    b=INO.index('bool startCarouselSlot(uint8_t slot)',a)
    return INO[a:b]

def test_b209_image_loader_compiles_with_minimal_host_stubs():
    cc=shutil.which('g++') or shutil.which('c++')
    assert cc
    code=_extract()
    stub=r'''
#include <cstddef>
#include <cstdint>
#include <cstring>
using std::size_t;
#define NUM_LEDS 4096
#define IDOTMATRIX_MEMORY_TELEMETRY 1

template<class T> T min(T a,T b){ return a < b ? a : b; }
struct CRGB { uint8_t r=0,g=0,b=0; CRGB()=default; CRGB(uint8_t R,uint8_t G,uint8_t B):r(R),g(G),b(B){} };
CRGB framebuffer[NUM_LEDS];
struct File {
  bool good=true; uint32_t n=0;
  size_t size() const { return n; }
  size_t read(uint8_t *p,size_t len){ std::memset(p,0,len); return len; }
  void close(){}
  explicit operator bool() const { return good; }
};
struct LittleFSClass { File open(const char*,const char*) { File f; f.n=12288; return f; } } LittleFS;
bool littleFsReady=true;
void refreshMatrix(){}
bool decodeSchedulePNG(File&,uint32_t){ return true; }
void idotMemoryTelemetrySnapshot(const char*){}
void idotMemoryTelemetryLatency(const char*,uint32_t){}
uint32_t micros(){ return 100; }
'''
    with tempfile.TemporaryDirectory() as td:
        p=Path(td)/'stub.cpp'
        p.write_text(stub+'\n'+code+'\nint main(){ return 0; }\n',encoding='utf-8')
        subprocess.run([cc,'-std=c++17','-Wall','-Wextra','-Werror','-fsyntax-only',str(p)],check=True)
