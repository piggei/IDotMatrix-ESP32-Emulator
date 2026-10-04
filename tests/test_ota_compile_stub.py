from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"


def test_ota_translation_unit_syntax_with_minimal_arduino_stubs():
    compiler = shutil.which("g++") or shutil.which("c++")
    assert compiler, "host C++ compiler not available"

    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        (td / "Arduino.h").write_text(r'''
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define F(x) x
class String {
public:
  std::string s;
  String() = default;
  String(const char *v): s(v ? v : "") {}
  String(const std::string &v): s(v) {}
  String(unsigned long v): s(std::to_string(v)) {}
  String(unsigned int v): s(std::to_string(v)) {}
  void reserve(std::size_t) {}
  const char *c_str() const { return s.c_str(); }
  String &operator+=(const char *v) { s += (v ? v : ""); return *this; }
  String &operator+=(const String &v) { s += v.s; return *this; }
  String &operator+=(char v) { s += v; return *this; }
};
inline String operator+(const String &a, const char *b) { return String(a.s + (b ? b : "")); }
inline String operator+(const String &a, const String &b) { return String(a.s + b.s); }
inline String operator+(const char *a, const String &b) { return String(std::string(a ? a : "") + b.s); }
struct SerialClass {
  template<class T> void print(const T&) {}
  template<class T> void println(const T&) {}
  void println() {}
  void flush() {}
};
extern SerialClass Serial;
struct ESPClass { uint64_t getEfuseMac() const { return 0; } void restart() {} };
extern ESPClass ESP;
inline void pinMode(int,int) {}
inline int digitalRead(int) { return HIGH; }
inline uint32_t millis() { return 0; }
inline void delay(uint32_t) {}
''')
        (td / "WiFi.h").write_text(r'''
#pragma once
#include "Arduino.h"
#define WIFI_OFF 0
#define WIFI_AP 1
struct IPAddress {
  String toString() const { return String("192.168.4.1"); }
};
class WiFiClass {
public:
  void mode(int) {}
  void setSleep(bool) {}
  bool softAP(const char*, const char*) { return true; }
  IPAddress softAPIP() const { return {}; }
};
extern WiFiClass WiFi;
''')
        (td / "DNSServer.h").write_text(r'''
#pragma once
#include "Arduino.h"
#include "WiFi.h"
class DNSServer {
public:
  bool start(uint16_t, const char*, const IPAddress&) { return true; }
  void processNextRequest() {}
};
''')
        (td / "WebServer.h").write_text(r'''
#pragma once
#include "Arduino.h"
#include <cstddef>
#include <cstdint>
#define HTTP_GET 0
#define HTTP_POST 1
enum UploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload {
  UploadStatus status = UPLOAD_FILE_START;
  String filename;
  uint8_t *buf = nullptr;
  std::size_t currentSize = 0;
  std::size_t totalSize = 0;
};
class WebServer {
  HTTPUpload up;
public:
  explicit WebServer(int) {}
  template<class F> void on(const char*, int, F) {}
  template<class F, class U> void on(const char*, int, F, U) {}
  template<class F> void onNotFound(F) {}
  void send(int, const char*, const String&) {}
  void send(int, const char*, const char*) {}
  void sendHeader(const char*, const char*) {}
  void sendHeader(const char*, const String&, bool = false) {}
  HTTPUpload &upload() { return up; }
  void begin() {}
  void handleClient() {}
};
''')
        (td / "Update.h").write_text(r'''
#pragma once
#include "Arduino.h"
#include <cstddef>
#include <cstdint>
#define UPDATE_SIZE_UNKNOWN 0xffffffffu
#define U_FLASH 0
class UpdateClass {
public:
  bool begin(std::size_t, int) { return true; }
  bool hasError() const { return false; }
  std::size_t write(uint8_t*, std::size_t n) { return n; }
  bool end(bool) { return true; }
  void abort() {}
  template<class T> void printError(T&) {}
};
extern UpdateClass Update;
''')
        (td / "globals.cpp").write_text(r'''
#include "Arduino.h"
#include "WiFi.h"
#include "Update.h"
SerialClass Serial;
ESPClass ESP;
WiFiClass WiFi;
UpdateClass Update;
''')
        cmd = [
            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
            f"-I{td}", f"-I{SRC}",
            "-DIDOTMATRIX_DEFAULT_OTA_ENABLED=1",
            "-DIDOTMATRIX_DEFAULT_OTA_TRIGGER_PIN=0",
            "-DIDOTMATRIX_DEFAULT_OTA_TRIGGER_ACTIVE_LOW=1",
            str(SRC / "IDotMatrixOta.cpp"), str(td / "globals.cpp"),
        ]
        subprocess.run(cmd, check=True)
