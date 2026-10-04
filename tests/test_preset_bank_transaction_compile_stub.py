from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INO = (ROOT / "src" / "IDotMatrix.ino").read_text(encoding="utf-8")


def _extract():
    a = INO.index("String presetFileName(")
    b = INO.index("int8_t nextPresetSlot(", a)
    return INO[a:b]


def test_preset_bank_transaction_helpers_compile_with_minimal_stubs():
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
#define PRESET_PROTOCOL_DEBUG 0
#define PRESET_SLOT_COUNT 6
#define PRESET_SLOT_BASE 14U
#define PRESET_DWELL_MS 3000UL
class String {
public:
  std::string s;
  String() = default;
  String(const char *v): s(v ? v : "") {}
  String(const std::string &v): s(v) {}
  const char *c_str() const { return s.c_str(); }
  String operator+(unsigned v) const { return String(s + std::to_string(v)); }
  String operator+(const char *v) const { return String(s + (v ? v : "")); }
};
struct LittleFSClass {
  bool exists(const String&) const { return true; }
  bool remove(const String&) { return true; }
  bool rename(const String&,const String&) { return true; }
} LittleFS;
struct SerialClass {
  template<class T> void print(const T&) {}
  template<class T,class U> void print(const T&,const U&) {}
  template<class T> void println(const T&) {}
  void println() {}
} Serial;
struct PresetSlotMeta {
  uint8_t configured=0; uint8_t dataType=0; uint16_t timeSign=0; uint32_t mediaSize=0; uint32_t mediaCRC=0;
};
struct DeferredPresetActivationState { bool active=false; uint8_t count=0; uint8_t order[PRESET_SLOT_COUNT]={}; };
enum DeferredAssetCommitKind : uint8_t { DEFERRED_ASSET_NONE=0, DEFERRED_ASSET_CAROUSEL=1, DEFERRED_ASSET_PRESET=2 };
struct DeferredAssetCommitState { bool active=false; DeferredAssetCommitKind kind=DEFERRED_ASSET_NONE; };
struct BulkTransferState { bool presetToFS=false; };
PresetSlotMeta presetSlots[PRESET_SLOT_COUNT];
PresetSlotMeta presetStaging[PRESET_SLOT_COUNT];
uint8_t presetOrder[PRESET_SLOT_COUNT]={};
uint8_t presetOrderCount=0;
bool presetActive=false;
int8_t presetActiveSlot=-1;
uint32_t presetSlotHoldMs=PRESET_DWELL_MS;
bool gifPresetPlaybackFileActive=false;
bool presetTransferIndicatorActive=false;
uint16_t presetStagedMask=0;
bool presetBankTxnActive=false;
bool presetBankCleanupPending=false;
uint8_t presetBankCleanupReason=0;
bool presetBankPreviousActive=false;
int8_t presetBankPreviousActiveSlot=-1;
DeferredPresetActivationState deferredPresetActivation;
DeferredAssetCommitState deferredAssetCommit;
BulkTransferState bulk;
bool littleFsReady=true;
int displayMode=0;
constexpr int DISPLAY_GIF=1, DISPLAY_TEXT=2, DISPLAY_RAW=3;
inline void gifCacheInvalidatePath(const char*) {}
inline bool fileMatchesMedia(const String&,uint32_t,uint32_t){ return true; }
inline void stopGIFPlayback() {}
inline void endCarouselTransferIndicator() {}
inline void sendCommandAck(uint8_t,uint8_t) {}
inline int8_t nextPresetSlot(int8_t){ return 0; }
inline bool startPresetSlot(uint8_t){ return true; }
'''
    with tempfile.TemporaryDirectory() as td_raw:
        td=Path(td_raw)
        (td/"stub.cpp").write_text(stub + "\n" + code + "\nint main(){ return 0; }\n", encoding="utf-8")
        subprocess.run(
            [compiler,"-std=c++17","-Wall","-Wextra","-Werror","-fsyntax-only",str(td/"stub.cpp")],
            check=True,
        )
