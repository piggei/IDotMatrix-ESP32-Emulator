from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _extract():
    a = INO.index("String carouselBankBackupFileName(")
    b = INO.index("bool saveCarouselSlotMeta(", a)
    return INO[a:b]


def test_carousel_bank_transaction_helpers_compile_with_minimal_stubs():
    compiler = shutil.which("g++") or shutil.which("c++")
    assert compiler, "host C++ compiler not available"
    code = _extract()
    stub = r'''
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
using std::size_t;
#define DEBUG_SERIAL 1
#define HEX 16
#define CAROUSEL_SLOT_COUNT 12
class String {
public:
  std::string s;
  String() = default;
  String(const char *v): s(v ? v : "") {}
  String(const std::string &v): s(v) {}
  const char *c_str() const { return s.c_str(); }
  bool operator!=(const String &o) const { return s != o.s; }
  String operator+(unsigned v) const { return String(s + std::to_string(v)); }
  String operator+(const char *v) const { return String(s + (v ? v : "")); }
};
class File {
public:
  bool good=true; size_t sz=0;
  explicit operator bool() const { return good; }
  size_t size() const { return sz; }
  size_t write(const uint8_t*,size_t n){ sz=n; return n; }
  size_t read(uint8_t *p,size_t n){ std::memset(p,0,n); return n; }
  void flush(){}
  void close(){}
};
struct LittleFSClass {
  bool exists(const String&) const { return true; }
  bool exists(const char*) const { return true; }
  bool remove(const String&) { return true; }
  bool remove(const char*) { return true; }
  bool rename(const String&,const String&) { return true; }
  bool rename(const char*,const char*) { return true; }
  File open(const String&,const char*) { return File(); }
  File open(const char*,const char*) { return File(); }
} LittleFS;
struct Preferences {
  size_t putUChar(const char*,uint8_t){ return 1; }
  size_t putBytes(const char*,const void*,size_t n){ return n; }
  bool isKey(const char*) const { return false; }
  bool remove(const char*) { return true; }
};
struct SerialClass {
  template<class T> void print(const T&) {}
  template<class T,class U> void print(const T&,const U&) {}
  template<class T> void println(const T&) {}
  template<class T,class U> void println(const T&,const U&) {}
} Serial;
struct __attribute__((packed)) CarouselSlotMeta {
  uint8_t configured=0; uint8_t dataType=0; uint16_t dwellSeconds=0; uint32_t mediaSize=0; uint32_t mediaCRC=0;
};
constexpr uint32_t CAROUSEL_BANK_TXN_MAGIC=0x43425458UL;
constexpr uint16_t CAROUSEL_BANK_TXN_VERSION=1U;
const char *CAROUSEL_BANK_TXN_FILE="/carbank.txn";
const char *CAROUSEL_BANK_TXN_TMP_FILE="/carbank.tmp";
struct __attribute__((packed)) CarouselBankTxnManifest {
  uint32_t magic=CAROUSEL_BANK_TXN_MAGIC; uint16_t version=CAROUSEL_BANK_TXN_VERSION; uint16_t bytes=0;
  uint16_t touchedMask=0; uint16_t mediaPresentMask=0; uint8_t orderCount=0; uint8_t order[CAROUSEL_SLOT_COUNT]={};
  CarouselSlotMeta slots[CAROUSEL_SLOT_COUNT]; uint32_t crc=0;
};
CarouselBankTxnManifest carouselBankTxnManifest;
bool carouselBankTxnActive=false;
bool littleFsReady=true;
bool carouselPrefsReady=true;
Preferences carouselPrefs;
uint8_t carouselOrderCount=0;
uint8_t carouselOrder[CAROUSEL_SLOT_COUNT]={};
CarouselSlotMeta carouselSlots[CAROUSEL_SLOT_COUNT];
inline uint32_t crc32Update(uint32_t crc,const uint8_t*,size_t){ return crc; }
inline String carouselFileName(uint8_t slot,uint8_t type){ return String("/car") + (unsigned)slot + (type==3 ? ".txt" : ".gif"); }
inline String carouselTempFileName(uint8_t slot){ return String("/car") + (unsigned)slot + ".tmp"; }
inline String carouselBackupFileName(uint8_t slot){ return String("/car") + (unsigned)slot + ".bak"; }
inline void gifCacheInvalidatePath(const char*){}
'''
    with tempfile.TemporaryDirectory() as td_raw:
        td=Path(td_raw)
        (td/"stub.cpp").write_text(stub + "\n" + code + "\nint main(){ return 0; }\n", encoding="utf-8")
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsyntax-only",str(td/"stub.cpp")],check=True)
