from pathlib import Path
import shutil, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[1]
INO=(ROOT/'src'/'IDotMatrix.ino').read_text(encoding='utf-8')


def _extract():
    a=INO.index('__attribute__((noinline)) uint8_t *allocateCarouselImageRxBuffer')
    b=INO.index('// Graffiti full-raster transfer',a)
    return INO[a:b]


def test_b210_buffer_helpers_compile_with_minimal_host_stubs():
    cc=shutil.which('g++') or shutil.which('c++')
    assert cc
    code=_extract()
    stub=r'''
#include <cstddef>
#include <cstdint>
#include <cstdlib>
using std::size_t;
#define DEBUG_SERIAL 1
#define BULK_PROTOCOL_DEBUG 1
#define IDOTMATRIX_BOARD_WAVESHARE_S3_RGB_MATRIX 1
#define MALLOC_CAP_SPIRAM 0x1
#define MALLOC_CAP_8BIT 0x2
struct BulkState { int8_t carouselLocalSlot=-1; bool carouselBuffered=false; const char *format="PNG"; } bulk;
struct SerialStub {
  void println(const char*){}
  void println(uint16_t){}
  void print(const char*){}
  void print(uint8_t){}
  void print(uint16_t){}
  void print(const char*, int){}
} Serial;
uint8_t *carouselImageRxData=nullptr;
size_t carouselImageRxWriteOffset=0;
void beginCarouselTransferIndicator(uint32_t,uint8_t){}
void *heap_caps_malloc(size_t n,uint32_t){ return std::malloc(n); }
'''
    with tempfile.TemporaryDirectory() as td:
        p=Path(td)/'stub.cpp'
        p.write_text(stub+'\n'+code+'\nint main(){ return beginCarouselBufferedImageTransfer(11296,0,5)?0:1; }\n',encoding='utf-8')
        subprocess.run([cc,'-std=c++17','-Wall','-Wextra','-Werror','-fsyntax-only',str(p)],check=True)
