#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ======================================================
// FIRMWARE RELEASE / BUILD ID
// FW_RELEASE identifies the public project release.
// FW_BUILD is the internal incremental development identifier.
// ======================================================
#define FW_RELEASE "0.5.0-dev"
#define FW_RELEASE_MAJOR 0
#define FW_RELEASE_MINOR 5
#define FW_BUILD 141

// Temporary hardware-test aid: when the official app changes screen power
// from OFF to ON, briefly overlay the release/build identifier without
// modifying the logical framebuffer or the active display mode.
#define APP_POWER_VERSION_SPLASH 1
#define VERSION_SPLASH_MS 1800UL
#define IDOT_STRINGIFY_INNER(x) #x
#define IDOT_STRINGIFY(x) IDOT_STRINGIFY_INNER(x)
#define PNG_DIAG_SERIAL 0
#define TEXT_PROTOCOL_DEBUG 0
#define BULK_PROTOCOL_DEBUG 0
#define PRESET_PROTOCOL_DEBUG 0  // Set to 1 only for Preset/Default protocol tracing (volatile slots 14..19).
#define CAROUSEL_PROTOCOL_DEBUG 0  // Set to 1 only for Device Assets protocol tracing.
#define DEVICE_INFO_PROTOCOL_DEBUG 0  // Set to 1 while studying MCU-version encoding in the 9-byte Device Info response.
#define CAROUSEL_SLOT_COUNT 12
#define CAROUSEL_DEFAULT_DWELL_SEC 5U
#define CAROUSEL_UPLOAD_SETTLE_MS 3000UL
#define PRESET_SLOT_BASE 14U
#define PRESET_SLOT_COUNT 6U
#define PRESET_DWELL_MS 3000UL

// Optional external RTC (DS3231 via RTClib).
// Keep 0 when no RTC hardware is installed: alarms use BLE time sync.
#define RTC_ENABLED             0
struct ScheduleActivity;
struct AlarmSlot;
#define RTC_SYNC_FROM_BLE       1

// ======================================================
// GENERIC REPOSITORY HARDWARE DEFAULTS
// ======================================================
#define DEBUG_SERIAL        1
#ifndef DEBUG_SERIAL_VERBOSE
#define DEBUG_SERIAL_VERBOSE 0
#endif

// Try a normal LittleFS mount first; format and retry only after mount failure.
#define LITTLEFS_FORMAT_ON_MOUNT_FAIL 1

// WS2812/FastLED backend pin. Ignored by HUB75 builds.
#ifndef MATRIX_PIN
#define MATRIX_PIN           4
#endif

// Optional external status LED. -1 disables it.
#define STATUS_LED_PIN      -1

// Optional SSD1306 diagnostic display. Disabled by default.
#define OLED_STATUS_ENABLED 0
#define OLED_SDA            4
#define OLED_SCL            15
#define OLED_RST            16
#define OLED_ROTATION       U8G2_R0
#define OLED_UNKNOWN_ALERT_MS 8000UL
#define OLED_UNKNOWN_BYTES    12

// Last unhandled FA02 command: kept independently from Serial so the
// on-board OLED can act as the protocol diagnostic console.
bool unknownCommandActive = false;
uint32_t unknownCommandAt = 0;
uint32_t unknownCommandCount = 0;
uint16_t unknownCommandLen = 0;
uint8_t unknownCommandData[OLED_UNKNOWN_BYTES] = {0};
uint8_t unknownCommandStored = 0;

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <FastLED.h>
#include <AnimatedGIF.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_partition.h>
#if OLED_STATUS_ENABLED
  #include <U8g2lib.h>
  // Configuration identical to the DollaTek sketch already verified on hardware.
  U8G2_SSD1306_128X64_NONAME_F_SW_I2C statusOLED(OLED_ROTATION, OLED_SCL, OLED_SDA, OLED_RST);
  bool statusOLEDReady=false;
#endif

// Schedule PNG: use the miniz inflater already provided by the ESP32 ROM/SDK,
// avoiding an additional external PNG library.
#if __has_include(<rom/miniz.h>)
  #include <rom/miniz.h>
#elif __has_include(<miniz.h>)
  #include <miniz.h>
#else
  #error "miniz header not found: required for Schedule PNG decoding"
#endif
#if RTC_ENABLED
  #include <Wire.h>
  #include <RTClib.h>
  RTC_DS3231 rtc;
  bool rtcReady = false;
  bool rtcTimeValid = false;
#endif


bool littleFsReady = false;

#if DEBUG_SERIAL
  #define DBG_BEGIN(x)       Serial.begin(x)
  #define DBG_PRINT(x)       Serial.print(x)
  #define DBG_PRINTLN(x)     Serial.println(x)
#else
  #define DBG_BEGIN(x)       do {} while (0)
  #define DBG_PRINT(x)       do {} while (0)
  #define DBG_PRINTLN(x)     do {} while (0)
#endif

#if PNG_DIAG_SERIAL
  #define PDBG_BEGIN() Serial.begin(115200)
  #define PDBG(x) Serial.print(x)
  #define PDBGLN(x) Serial.println(x)
#else
  #define PDBG_BEGIN() do {} while (0)
  #define PDBG(x) do {} while (0)
  #define PDBGLN(x) do {} while (0)
#endif

#define DEVICE_NAME "IDM-858931"

// -----------------------------------------------------------------------------
// Display architecture
//
// LOGICAL resolution is the iDotMatrix model exposed to the official app.
// PHYSICAL resolution is the actually connected display. They are independent.
// The final output stage automatically performs 1:1 copy, nearest-neighbour
// upscaling or box-average downscaling for every 16/32/64 combination.
// -----------------------------------------------------------------------------
#define DISPLAY_BACKEND_WS2812  0
#define DISPLAY_BACKEND_HUB75   1

// BUILD 125: these defaults remain Arduino-IDE friendly, while PlatformIO
// environments can override them with build_flags without editing this file.
#ifndef DISPLAY_BACKEND
#define DISPLAY_BACKEND DISPLAY_BACKEND_HUB75
#endif

// iDotMatrix logical profile: 1=16x16, 3=32x32, 4=64x64.
#ifndef IDOTMATRIX_SCREEN_TYPE
#define IDOTMATRIX_SCREEN_TYPE  4
#endif

#if IDOTMATRIX_SCREEN_TYPE == 1
  #define MATRIX_WIDTH   16
  #define MATRIX_HEIGHT  16
#elif IDOTMATRIX_SCREEN_TYPE == 3
  #define MATRIX_WIDTH   32
  #define MATRIX_HEIGHT  32
#elif IDOTMATRIX_SCREEN_TYPE == 4
  #define MATRIX_WIDTH   64
  #define MATRIX_HEIGHT  64
#else
  #error "Unsupported IDOTMATRIX_SCREEN_TYPE (supported: 1=16x16, 3=32x32, 4=64x64)"
#endif

#define NUM_LEDS       ((uint16_t)MATRIX_WIDTH * (uint16_t)MATRIX_HEIGHT)
#define LOGICAL_FRAME_BYTES ((size_t)NUM_LEDS * sizeof(CRGB))

// Physical display dimensions. These do not change what the app sees.
#ifndef PHYSICAL_MATRIX_WIDTH
#define PHYSICAL_MATRIX_WIDTH   64
#endif
#ifndef PHYSICAL_MATRIX_HEIGHT
#define PHYSICAL_MATRIX_HEIGHT  64
#endif
#define PHYSICAL_NUM_LEDS       ((uint16_t)PHYSICAL_MATRIX_WIDTH * (uint16_t)PHYSICAL_MATRIX_HEIGHT)

// WS2812 backend options.
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define MATRIX_MIRROR_X 1

// MatrixPortal S3 / HUB75 backend. BUILD 130 deliberately mirrors the
// native WLED MatrixPortal path: ESP32-HUB75-MatrixPanel-I2S-DMA, 8-bit
// color depth, single buffering and the exact MatrixPortal S3 HUB75 pinout.
// Keeping brightness in the HUB75 OE/PWM path preserves the full RGB values
// instead of pre-scaling the framebuffer to the local brightness ceiling.
#if DISPLAY_BACKEND == DISPLAY_BACKEND_HUB75
  #include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
  HUB75_I2S_CFG::i2s_pins hub75Pins = {
    42, 41, 40, 38, 39, 37, // R1, G1, B1, R2, G2, B2
    45, 36, 48, 35, 21,     // A, B, C, D, E
    47, 14, 2                // LAT, OE, CLK
  };
  #define HUB75_COLOR_DEPTH 8
  MatrixPanel_I2S_DMA *hub75Matrix = nullptr;
#endif

// Local hardware/test ceiling expressed on the common 0..255 brightness scale.
// WS2812 applies it through FastLED global brightness. HUB75 applies it through
// MatrixPanel_I2S_DMA output-enable timing so framebuffer RGB values stay 8-bit.
#define MAX_LED_BRIGHTNESS 50

// Persistenza luminosita
#define BRIGHTNESS_NVS_NAMESPACE "idotmatrix"
#define BRIGHTNESS_NVS_KEY       "brightness"
#define BRIGHTNESS_SAVE_DELAY_MS 1000UL

extern uint8_t brightnessPercent;
bool brightnessDirty = false;
uint32_t brightnessDirtySince = 0;

uint8_t brightnessPercentToHw(uint8_t percent) {
  if (percent > 100) percent = 100;
  return (uint8_t)((uint16_t)percent * MAX_LED_BRIGHTNESS / 100U);
}

void applyCurrentBrightness() {
  uint8_t hw = brightnessPercentToHw(brightnessPercent);
#if DISPLAY_BACKEND == DISPLAY_BACKEND_WS2812
  FastLED.setBrightness(hw);
#endif
#if DEBUG_SERIAL
  Serial.print("BRIGHTNESS HW: ");
  Serial.print(brightnessPercent);
  Serial.print("% -> ");
  Serial.print(hw);
  Serial.print("/");
  Serial.println(MAX_LED_BRIGHTNESS);
#endif
}

void scheduleBrightnessSave() {
  brightnessDirty = true;
  brightnessDirtySince = millis();
}

void flushBrightnessSaveIfNeeded() {
  if (!brightnessDirty) return;
  if ((uint32_t)(millis() - brightnessDirtySince) < BRIGHTNESS_SAVE_DELAY_MS) return;

  Preferences prefs;
  if (prefs.begin(BRIGHTNESS_NVS_NAMESPACE, false)) {
    prefs.putUChar(BRIGHTNESS_NVS_KEY, brightnessPercent);
    prefs.end();
#if DEBUG_SERIAL
    Serial.print("BRIGHTNESS SAVE: ");
    Serial.print(brightnessPercent);
    Serial.println("%");
#endif
  }
  brightnessDirty = false;
}

void loadBrightnessFromNVS() {
  Preferences prefs;
  if (prefs.begin(BRIGHTNESS_NVS_NAMESPACE, true)) {
    if (prefs.isKey(BRIGHTNESS_NVS_KEY)) {
      brightnessPercent = prefs.getUChar(BRIGHTNESS_NVS_KEY, 100);
    }
    prefs.end();
  }
  if (brightnessPercent > 100) brightnessPercent = 100;
#if DEBUG_SERIAL
  Serial.print("BRIGHTNESS LOAD: ");
  Serial.print(brightnessPercent);
  Serial.println("%");
#endif
  applyCurrentBrightness();
}

#define MAX_PACKET_SIZE     8192
#define MAX_TEXT_PAYLOAD   16654  // 14-byte header + up to 64 x (4 meta + 256 bitmap)
#define MAX_TEXT_GLYPHS        64
#define TEXT_GLOBAL_HEADER     14
#define TEXT_GLYPH_META        4   // marker + RGB; glyph bitmap follows
#define TEXT_MAX_BITMAP_BYTES 256  // 32x64 glyph = 256 bytes
#define MAX_EFFECT_COLORS   16
#define PACKET_REASSEMBLY_TIMEOUT_MS  5000UL
#define BULK_TRANSFER_TIMEOUT_MS      30000UL

// ======================================================
// ALARMS
// ======================================================
#define ALARM_SLOT_COUNT       10
#define ALARM_BUZZER_ENABLED   0
#define ALARM_BUZZER_PIN       -1
#define BUZZER_ACTIVE_HIGH     1

// Active-buzzer trill pattern: 3 short beeps followed by a longer pause.
// Fully non-blocking: BLE, animations and matrix refresh continue normally.
#define BUZZER_PULSE_ON_MS       90UL
#define BUZZER_PULSE_GAP_MS      70UL
#define BUZZER_TRILL_PAUSE_MS   550UL
#define BUZZER_TRILL_PULSES       3
#define COUNTDOWN_BUZZER_ENABLED   0
#define CONNECTION_BUZZER_ENABLED  0
#define ALARM_MEDIA_BASE_ID    0x14
#define ALARM_CONTENT_GIF      0x01
#define ALARM_CONTENT_RAW      0x02
#define ALARM_HEADER_SIZE      24
#define ALARM_UPLOAD_TIMEOUT_MS 10000UL

// ======================================================
// PROGRAM / SCHEDULE
// ======================================================
#define SCHEDULE_MAX_ACTIVITIES 32
#define SCHEDULE_COMMIT_DELAY_MS 900UL
#define SCHEDULE_MEDIA_UPLOAD_TIMEOUT_MS 10000UL
#define SCHEDULE_CONTENT_GIF    0x01
#define SCHEDULE_CONTENT_IMAGE  0x02
#define SCHEDULE_CONTENT_TEXT   0x03
#define SCHEDULE_MEDIA_BASE_ID  0x1E
#define SCHEDULE_BUZZER_ENABLED 0
#define SCHEDULE_BUZZER_PIN     ALARM_BUZZER_PIN



#define FA_SERVICE_UUID "000000fa-0000-1000-8000-00805f9b34fb"
#define FA02_UUID       "0000fa02-0000-1000-8000-00805f9b34fb"
#define FA03_UUID       "0000fa03-0000-1000-8000-00805f9b34fb"
#define AE_SERVICE_UUID "0000ae00-0000-1000-8000-00805f9b34fb"
#define AE01_UUID       "0000ae01-0000-1000-8000-00805f9b34fb"
#define AE02_UUID       "0000ae02-0000-1000-8000-00805f9b34fb"

BLEServer *server = nullptr;
BLECharacteristic *fa02 = nullptr;
BLECharacteristic *fa03 = nullptr;
BLECharacteristic *ae01 = nullptr;
BLECharacteristic *ae02 = nullptr;
bool deviceConnected = false;

// BUILD 86: BLE callbacks and Arduino loop() run on different FreeRTOS tasks
// (and typically different ESP32 cores). Protect shared protocol/runtime state
// with a task mutex instead of relying on volatile flags. This is a scheduler
// mutex, not a spinlock: filesystem and renderer work must never run inside an
// interrupt/critical section.
SemaphoreHandle_t runtimeStateMutex = nullptr;

bool lockRuntimeState() {
  return runtimeStateMutex && xSemaphoreTake(runtimeStateMutex, portMAX_DELAY) == pdTRUE;
}

void unlockRuntimeState() {
  if (runtimeStateMutex) xSemaphoreGive(runtimeStateMutex);
}

#if DISPLAY_BACKEND == DISPLAY_BACKEND_WS2812
CRGB leds[PHYSICAL_NUM_LEDS];
#endif
// Logical buffers are allocated from the heap at runtime. Keeping 64x64
// framebuffers in .bss overflows ESP32 DRAM before the sketch can link.
CRGB *framebuffer = nullptr;

enum DisplayMode {
  DISPLAY_NONE,
  DISPLAY_SOLID,
  DISPLAY_RAW,
  DISPLAY_GRAFFITI,
  DISPLAY_GIF,
  DISPLAY_TEXT,
  DISPLAY_EFFECT,
  DISPLAY_AUDIO,
  DISPLAY_CLOCK,
  DISPLAY_COUNTDOWN,
  DISPLAY_STOPWATCH,
  DISPLAY_SCOREBOARD
};

DisplayMode displayMode = DISPLAY_NONE;
bool screenOn = false;
bool versionSplashActive = false;
uint32_t versionSplashUntil = 0;
bool diyMode = false;
bool flipped180 = false;
uint8_t brightnessPercent = 100;

// ======================================================
// CLOCK / DATE
// ======================================================
bool clockSynced = false;
uint16_t syncYear = 2026;
uint8_t syncMonth = 1, syncDay = 1;
uint8_t syncHour = 0, syncMinute = 0, syncSecond = 0;
uint32_t syncMillis = 0;
uint8_t clockStyle = 0;
bool clock24h = false;
bool clockShowDate = false;
CRGB clockColor = CRGB::White;
uint32_t clockCycleStartedAt = 0;

// ======================================================
// ECO
// ======================================================
struct EnergySavingState {
  bool enabled = false;
  uint8_t startHour = 0, startMinute = 0;
  uint8_t endHour = 0, endMinute = 0;
  uint8_t reductionPercent = 0;
} energySaving;

// Last brightness actually pushed to FastLED. This lets static display modes
// react when an ECO interval begins/ends even if no renderer requests refresh.
uint8_t lastAppliedOutputBrightness = 0xFF;
uint32_t lastEnergySavingCheckMs = 0;

// ======================================================
// COUNTDOWN / STOPWATCH / SCOREBOARD
// ======================================================
bool countdownRunning = false;
bool countdownPaused = false;
uint32_t countdownRemainingMs = 0;
uint32_t countdownStartMillis = 0;
bool countdownFinishSent = false;

bool stopwatchRunning = false;
uint32_t stopwatchElapsedMs = 0;
uint32_t stopwatchStartMillis = 0;

uint16_t scoreA = 0, scoreB = 0;

// ======================================================
// TEXT
// ======================================================
struct TextState {
  bool valid = false;
  uint8_t glyphCount = 0;
  uint8_t motionEffect = 0;
  uint8_t speed = 5;
  uint8_t colorMode = 1;
  uint8_t colorR = 255, colorG = 255, colorB = 255;
  uint8_t backgroundMode = 0;
  uint8_t backgroundR = 0, backgroundG = 0, backgroundB = 0;
  uint8_t meta[MAX_TEXT_GLYPHS][TEXT_GLYPH_META];
  uint8_t bitmap[MAX_TEXT_GLYPHS][TEXT_MAX_BITMAP_BYTES];
  uint8_t glyphWidth = 8;
  uint8_t glyphHeight = 16;
  uint16_t glyphBytes = 16;
  uint8_t glyphAdvance = 8;
  int16_t offsetX = 0, offsetY = 1;
  uint32_t animationStart = 0;
  uint32_t lastFrame = 0;        // motion/page timing
  uint32_t lastRenderFrame = 0;  // visual refresh timing for dynamic colors/effects
} textState;

// ======================================================
// EFFECT LIGHT: 03 02 EFFECT SPEED COUNT [R G B]...
// RGB protocol values use the observed 0..127 scale.
// ======================================================
struct EffectState {
  bool valid = false;
  uint8_t effect = 0;
  uint8_t speed = 90;
  uint8_t colorCount = 0;
  CRGB colors[MAX_EFFECT_COLORS];
  uint32_t startMillis = 0;
  uint32_t lastFrameMillis = 0;
} effectState;

// ======================================================
// AUDIO / RHYTHM
// Five LEVEL modes and five FFT modes observed from app.
// LEVEL: 06 00 00 02 LEVEL MODE, MODE=1..5
// FFT  : 21 00 01 02 MODE + 16 mirrored values, MODE=0..4
// ======================================================
struct AudioState {
  bool valid = false;
  bool fft = false;
  uint8_t mode = 0;
  uint8_t level = 0;
  uint8_t bands[8] = {0};
  uint32_t lastPacketMs = 0;
  uint32_t packetCounter = 0;
} audioState;


// ======================================================
// FORWARD DECLARATIONS USED BY ALARM SUPPORT
//
// Le routine delle sveglie sono definite prima del blocco GIF.
// Dichiarare qui simboli e funzioni evita di dipendere dalla
// generazione automatica dei prototipi dell'IDE Arduino.
// ======================================================
void freeGIF();
bool startGIF();
bool startStoredGIFPlayback(const String &sourcePath, uint32_t expectedSize, uint32_t expectedCRC);
void stopGIFPlayback();
void switchDisplayMode(DisplayMode m);
void renderClock();
void stopScheduleActivity();
extern uint8_t scheduleGlobalFlags;
extern int8_t scheduleActiveIndex;
extern int8_t scheduleFailedIndex;
extern CRGB *scheduleSavedFrame;
uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len);
bool fileMatchesMedia(const String &path, uint32_t expectedSize, uint32_t expectedCRC);
void clearFramebuffer(const CRGB &color = CRGB::Black);
bool handleCarouselCommand(const uint8_t *data, size_t len);
void loadCarousel();
void updateCarousel(uint32_t now);
void stopCarouselPlayback();
void updatePreset(uint32_t now);
void stopPresetPlayback();
void clearPresetBank(bool removeFiles);
void clearPersistentDeviceState();
void applyBootDisplayPolicy();
int8_t nextCarouselSlot(int8_t current);
bool startCarouselSlot(uint8_t slot);
extern bool carouselActive;
extern bool presetActive;
extern int8_t carouselActiveSlot;
extern bool gifCarouselPlaybackFileActive;
extern bool carouselEnterRequested;


// ======================================================
// ALARM STATE / PERSISTENCE
// ======================================================
struct AlarmSlot {
  bool configured = false;
  uint8_t flags = 0;          // bit0 enable, bit1..7 lun..dom
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t durationSec = 10;
  uint8_t reserved1 = 0;
  uint8_t contentType = 0;
  uint8_t buzzer = 0;
  uint8_t reserved2 = 0;
  uint32_t mediaSize = 0;
  uint32_t mediaCRC = 0;
  uint16_t reserved3 = 0;
  uint8_t mediaId = 0;
  uint32_t lastTriggerMinuteKey = 0xFFFFFFFFUL;
};

AlarmSlot alarms[ALARM_SLOT_COUNT];
Preferences alarmPrefs;
bool alarmActive = false;
uint8_t activeAlarmSlot = 0xFF;
uint32_t alarmEndsAt = 0;
DisplayMode alarmPreviousMode = DISPLAY_CLOCK;
bool alarmPreviousCarousel = false;
int8_t alarmPreviousCarouselSlot = -1;

String alarmFileName(uint8_t slot) { return String("/alarm") + slot + ".bin"; }
String alarmTempFileName(uint8_t slot) { return String("/alarm") + slot + ".tmp"; }
String alarmBackupFileName(uint8_t slot) { return String("/alarm") + slot + ".bak"; }

// BUILD 132: 64x64 Alarm media can span multiple complete FA02 logical
// packets. Each packet repeats the same 24-byte Alarm header; mediaSize and
// mediaCRC describe the complete media object, not the payload bytes carried
// by the current packet. Stage chunks in LittleFS and publish atomically only
// after the declared total size and CRC have been verified.
struct AlarmUploadState {
  bool active = false;
  uint8_t slot = 0xFF;
  AlarmSlot candidate;
  AlarmSlot previous;
  uint32_t received = 0;
  uint32_t runningCRC = 0xFFFFFFFFUL;
  uint32_t lastChunkAt = 0;
};

AlarmUploadState alarmUpload;

bool alarmUploadMetadataMatches(const AlarmSlot &a, const AlarmSlot &b) {
  return a.flags == b.flags &&
         a.hour == b.hour &&
         a.minute == b.minute &&
         a.durationSec == b.durationSec &&
         a.reserved1 == b.reserved1 &&
         a.contentType == b.contentType &&
         a.buzzer == b.buzzer &&
         a.mediaSize == b.mediaSize &&
         a.mediaCRC == b.mediaCRC &&
         a.reserved3 == b.reserved3 &&
         a.mediaId == b.mediaId;
}

void abortAlarmUpload(const char *reason) {
  if (!alarmUpload.active) return;
#if DEBUG_SERIAL
  if (reason) {
    Serial.print("ALARM UPLOAD ABORT slot="); Serial.print(alarmUpload.slot);
    Serial.print(" received="); Serial.print(alarmUpload.received);
    Serial.print("/"); Serial.print(alarmUpload.candidate.mediaSize);
    Serial.print(" reason="); Serial.println(reason);
  }
#endif
  if (littleFsReady && alarmUpload.slot < ALARM_SLOT_COUNT)
    LittleFS.remove(alarmTempFileName(alarmUpload.slot));
  alarmUpload = AlarmUploadState();
}

bool beginAlarmUpload(uint8_t slot, const AlarmSlot &candidate) {
  abortAlarmUpload(nullptr);
  if (!littleFsReady || slot >= ALARM_SLOT_COUNT) return false;

  String tmp = alarmTempFileName(slot);
  LittleFS.remove(tmp);
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  f.close();

  alarmUpload.active = true;
  alarmUpload.slot = slot;
  alarmUpload.candidate = candidate;
  alarmUpload.previous = alarms[slot];
  alarmUpload.received = 0;
  alarmUpload.runningCRC = 0xFFFFFFFFUL;
  alarmUpload.lastChunkAt = millis();
  return true;
}

void updateAlarmUploadTimeout() {
  if (alarmUpload.active &&
      (uint32_t)(millis() - alarmUpload.lastChunkAt) > ALARM_UPLOAD_TIMEOUT_MS)
    abortAlarmUpload("timeout");
}

bool saveAlarmMetaValue(uint8_t slot, const AlarmSlot &value) {
  if (slot >= ALARM_SLOT_COUNT) return false;
  char key[12];
  snprintf(key,sizeof(key),"a%u",slot);
  return alarmPrefs.putBytes(key,&value,sizeof(AlarmSlot)) == sizeof(AlarmSlot);
}

bool saveAlarmMeta(uint8_t slot) {
  return slot < ALARM_SLOT_COUNT && saveAlarmMetaValue(slot, alarms[slot]);
}

void recoverAlarmFile(uint8_t slot) {
  if (!littleFsReady || slot >= ALARM_SLOT_COUNT) return;
  const AlarmSlot &a = alarms[slot];
  String dst = alarmFileName(slot), tmp = alarmTempFileName(slot), bak = alarmBackupFileName(slot);
  LittleFS.remove(tmp);

  if (!a.configured || !a.mediaSize) {
    LittleFS.remove(dst);
    LittleFS.remove(bak);
    return;
  }

  bool dstOK = fileMatchesMedia(dst, a.mediaSize, a.mediaCRC);
  bool bakOK = fileMatchesMedia(bak, a.mediaSize, a.mediaCRC);
  if (dstOK) {
    LittleFS.remove(bak);
  } else if (bakOK) {
    LittleFS.remove(dst);
    if (!LittleFS.rename(bak, dst)) {
#if DEBUG_SERIAL
      Serial.print("ALARM RECOVERY rename failed slot="); Serial.println(slot);
#endif
    }
  }
}

void loadAlarms() {
  alarmPrefs.begin("idot-alarm",false);
  for(uint8_t i=0;i<ALARM_SLOT_COUNT;i++){
    char key[12]; snprintf(key,sizeof(key),"a%u",i);
    size_t n=alarmPrefs.getBytesLength(key);
    if(n==sizeof(AlarmSlot)) alarmPrefs.getBytes(key,&alarms[i],sizeof(AlarmSlot));
    alarms[i].lastTriggerMinuteKey=0xFFFFFFFFUL;
    recoverAlarmFile(i);
#if DEBUG_SERIAL
    if(alarms[i].configured){
      Serial.print("ALARM LOAD slot="); Serial.print(i);
      Serial.print(" flags=0x"); Serial.print(alarms[i].flags,HEX);
      Serial.print(" enabled="); Serial.print((alarms[i].flags&0x01)?1:0);
      Serial.print(" time="); if(alarms[i].hour<10)Serial.print('0'); Serial.print(alarms[i].hour);
      Serial.print(':'); if(alarms[i].minute<10)Serial.print('0'); Serial.print(alarms[i].minute);
      Serial.print(" dur="); Serial.print(alarms[i].durationSec);
      Serial.print(" type="); Serial.print(alarms[i].contentType);
      Serial.print(" buzzer="); Serial.print(alarms[i].buzzer);
      Serial.print(" bytes="); Serial.print(alarms[i].mediaSize);
      Serial.print(" mediaId=0x"); Serial.print(alarms[i].mediaId,HEX);
      Serial.print(" file=");
      Serial.println(!littleFsReady ? "FS-OFFLINE" : (LittleFS.exists(alarmFileName(i)) ? "YES" : "NO"));
    }
#endif
  }
}

bool isLeapYear(uint16_t y){
  return (y%4==0 && y%100!=0) || (y%400==0);
}

bool isValidDateTime(uint16_t y,uint8_t mo,uint8_t d,uint8_t h,uint8_t mi,uint8_t se){
  if(mo<1 || mo>12 || h>23 || mi>59 || se>59) return false;
  static const uint8_t mdays[]={31,28,31,30,31,30,31,31,30,31,30,31};
  uint8_t dim=mdays[mo-1];
  if(mo==2 && isLeapYear(y)) dim=29;
  return d>=1 && d<=dim;
}

uint8_t currentWeekdayBit(uint16_t y,uint8_t m,uint8_t d){
  // Sakamoto: 0=Sunday. Protocol: bit1=Monday ... bit7=Sunday.
  if(!isValidDateTime(y,m,d,0,0,0)) return 0;
  static const uint8_t t[]={0,3,2,5,0,3,5,1,4,6,2,4};
  uint16_t yy=y; if(m<3) yy--;
  uint8_t dow=(yy+yy/4-yy/100+yy/400+t[m-1]+d)%7;
  return dow==0 ? 0x80 : (uint8_t)(1U<<dow);
}

void getAlarmDateTime(uint16_t &y,uint8_t &mo,uint8_t &d,uint8_t &h,uint8_t &mi,uint8_t &se){
#if RTC_ENABLED
  if(rtcReady && rtcTimeValid){
    DateTime now=rtc.now();
    y=now.year(); mo=now.month(); d=now.day(); h=now.hour(); mi=now.minute(); se=now.second();
    return;
  }
#endif
  getCurrentTime(h,mi,se);
  y=syncYear; mo=syncMonth; d=syncDay;
  if(!clockSynced) return;
  uint32_t elapsed=(millis()-syncMillis)/1000UL;
  uint32_t dayCarry=((uint32_t)syncHour*3600UL+(uint32_t)syncMinute*60UL+syncSecond+elapsed)/86400UL;
  static const uint8_t mdays[]={31,28,31,30,31,30,31,31,30,31,30,31};
  while(dayCarry--){
    uint8_t dim=mdays[mo-1];
    if(mo==2&&isLeapYear(y)) dim=29;
    if(++d>dim){ d=1; if(++mo>12){mo=1;y++;} }
  }
}

bool loadAlarmMedia(uint8_t slot){
  if(!littleFsReady || slot>=ALARM_SLOT_COUNT) return false;
  AlarmSlot &a=alarms[slot];
  File f=LittleFS.open(alarmFileName(slot),"r");
  if(!f || (uint32_t)f.size()!=a.mediaSize){ if(f)f.close(); return false; }
  if(a.contentType==ALARM_CONTENT_RAW && a.mediaSize==(uint32_t)NUM_LEDS*3UL){
    uint8_t rgb[3];
    switchDisplayMode(DISPLAY_RAW);
    for(uint16_t i=0;i<NUM_LEDS;i++){ if(f.read(rgb,3)!=3){f.close();return false;} framebuffer[i]=CRGB(rgb[0],rgb[1],rgb[2]); }
    f.close(); refreshMatrix(); return true;
  }
  if(a.contentType==ALARM_CONTENT_GIF){
    f.close();
    return startStoredGIFPlayback(alarmFileName(slot), a.mediaSize, a.mediaCRC);
  }
  f.close(); return false;
}

// ======================================================
// ACTIVE BUZZER - NON-BLOCKING TRILL
// ======================================================
#if ALARM_BUZZER_ENABLED && (ALARM_BUZZER_PIN >= 0)
static bool buzzerOutputOn = false;
static bool buzzerPatternRunning = false;
static bool countdownBuzzerOneShot = false;
static bool scheduleBuzzerOneShot = false;
static bool connectionBuzzerOneShot = false;
static uint8_t buzzerPulseIndex = 0;
static uint32_t buzzerNextChangeAt = 0;

static inline void setBuzzerOutput(bool on) {
  buzzerOutputOn = on;
  digitalWrite(ALARM_BUZZER_PIN,
               on ? (BUZZER_ACTIVE_HIGH ? HIGH : LOW)
                  : (BUZZER_ACTIVE_HIGH ? LOW : HIGH));
}

static bool continuousBuzzerRequested() {
  bool wanted = false;

  if (alarmActive && activeAlarmSlot < ALARM_SLOT_COUNT)
    wanted |= alarms[activeAlarmSlot].buzzer != 0;

  return wanted;
}

void triggerCountdownFinishBuzzer() {
#if COUNTDOWN_BUZZER_ENABLED
  countdownBuzzerOneShot = true;
#endif
}

void cancelCountdownFinishBuzzer() {
  countdownBuzzerOneShot = false;
}

void triggerScheduleStartBuzzer() {
#if SCHEDULE_BUZZER_ENABLED && (SCHEDULE_BUZZER_PIN >= 0)
  scheduleBuzzerOneShot = true;
#endif
}

void cancelScheduleStartBuzzer() {
  scheduleBuzzerOneShot = false;
}

void triggerConnectionBuzzer() {
#if CONNECTION_BUZZER_ENABLED
  // Connection feedback is intentionally low priority. Never interrupt an
  // alarm or another one-shot notification already using the buzzer.
  if (!continuousBuzzerRequested() && !buzzerPatternRunning &&
      !countdownBuzzerOneShot && !scheduleBuzzerOneShot) {
    connectionBuzzerOneShot = true;
  }
#endif
}

void cancelConnectionBuzzer() {
  connectionBuzzerOneShot = false;
}

void updateBuzzer() {
  const bool continuousWanted = continuousBuzzerRequested();
  const bool trillOneShotWanted = countdownBuzzerOneShot || scheduleBuzzerOneShot;
  const bool wanted = continuousWanted || trillOneShotWanted || connectionBuzzerOneShot;
  const uint32_t now = millis();

  if (!wanted) {
    if (buzzerPatternRunning || buzzerOutputOn) setBuzzerOutput(false);
    buzzerPatternRunning = false;
    buzzerPulseIndex = 0;
    return;
  }

  if (!buzzerPatternRunning) {
    buzzerPatternRunning = true;
    buzzerPulseIndex = 0;
    setBuzzerOutput(true);
    buzzerNextChangeAt = now + BUZZER_PULSE_ON_MS;
    return;
  }

  if ((int32_t)(now - buzzerNextChangeAt) < 0) return;

  if (buzzerOutputOn) {
    setBuzzerOutput(false);

    // BLE connection feedback is exactly one short pulse, not a trill.
    if (connectionBuzzerOneShot) {
      connectionBuzzerOneShot = false;
      buzzerPatternRunning = false;
      buzzerPulseIndex = 0;
      return;
    }

    ++buzzerPulseIndex;

    if (buzzerPulseIndex >= BUZZER_TRILL_PULSES) {
      // Countdown completion and Schedule start are one-shot trills. Alarm
      // remains the only repeating buzzer request.
      countdownBuzzerOneShot = false;
      scheduleBuzzerOneShot = false;
      if (!continuousWanted) {
        buzzerPatternRunning = false;
        buzzerPulseIndex = 0;
        return;
      }
      buzzerNextChangeAt = now + BUZZER_TRILL_PAUSE_MS;
    } else {
      buzzerNextChangeAt = now + BUZZER_PULSE_GAP_MS;
    }
  } else {
    if (buzzerPulseIndex >= BUZZER_TRILL_PULSES) buzzerPulseIndex = 0;
    setBuzzerOutput(true);
    buzzerNextChangeAt = now + BUZZER_PULSE_ON_MS;
  }
}
#else
void triggerCountdownFinishBuzzer() {}
void cancelCountdownFinishBuzzer() {}
void triggerScheduleStartBuzzer() {}
void cancelScheduleStartBuzzer() {}
void triggerConnectionBuzzer() {}
void cancelConnectionBuzzer() {}
void updateBuzzer() {}
#endif

void startAlarm(uint8_t slot){
  if(slot>=ALARM_SLOT_COUNT || alarmActive) return;
  AlarmSlot &a=alarms[slot];

  // Alarm has priority over Schedule. Stop the Schedule first so its restore
  // path cannot tear down media that the Alarm has just started.
  if(scheduleActiveIndex>=0) stopScheduleActivity();
  if(presetActive) stopPresetPlayback();

  alarmPreviousMode=displayMode;
  alarmPreviousCarousel=carouselActive || gifCarouselPlaybackFileActive;
  alarmPreviousCarouselSlot=carouselActiveSlot;
  if(scheduleSavedFrame) ::memcpy(scheduleSavedFrame, framebuffer, LOGICAL_FRAME_BYTES);
  alarmActive=true; activeAlarmSlot=slot; alarmEndsAt=millis()+(uint32_t)a.durationSec*1000UL;
  loadAlarmMedia(slot);
#if DEBUG_SERIAL
  Serial.print("ALARM TRIGGER slot=");Serial.print(slot);Serial.print(" duration=");Serial.print(a.durationSec);Serial.print(" buzzer=");Serial.println(a.buzzer);
#endif
}

void stopAlarm(){
  if(!alarmActive) return;
  alarmActive=false; activeAlarmSlot=0xFF;
  stopGIFPlayback();

  if(alarmPreviousCarousel && nextCarouselSlot(-1)>=0){
    carouselEnterRequested=true;
    int8_t slot=alarmPreviousCarouselSlot;
    if(slot<0 || slot>=CAROUSEL_SLOT_COUNT) slot=nextCarouselSlot(-1);
    alarmPreviousCarousel=false; alarmPreviousCarouselSlot=-1;
    if(slot>=0){ startCarouselSlot((uint8_t)slot); return; }
  }
  alarmPreviousCarousel=false; alarmPreviousCarouselSlot=-1;

  // Restore framebuffer-backed modes exactly. Other dynamic modes cannot be
  // reconstructed generically, so fall back to the synchronized clock/blank.
  if(scheduleSavedFrame && (alarmPreviousMode==DISPLAY_SOLID || alarmPreviousMode==DISPLAY_RAW ||
      alarmPreviousMode==DISPLAY_GRAFFITI)) {
    ::memcpy(framebuffer, scheduleSavedFrame, LOGICAL_FRAME_BYTES);
    displayMode=alarmPreviousMode;
    refreshMatrix();
  } else if(clockSynced) {
    switchDisplayMode(DISPLAY_CLOCK); renderClock();
  } else {
    switchDisplayMode(DISPLAY_NONE); clearFramebuffer(); refreshMatrix();
  }
}

void updateAlarms(){
  updateAlarmUploadTimeout();
  if(alarmActive){ if((int32_t)(millis()-alarmEndsAt)>=0) stopAlarm(); return; }
#if RTC_ENABLED
  if(!clockSynced && !(rtcReady && rtcTimeValid)) return;
#else
  if(!clockSynced) return;
#endif
  static uint32_t lastCheck=0; if(millis()-lastCheck<500) return; lastCheck=millis();
  uint16_t y; uint8_t mo,d,h,mi,se; getAlarmDateTime(y,mo,d,h,mi,se);
  uint32_t minuteKey=((uint32_t)y<<20)|((uint32_t)mo<<16)|((uint32_t)d<<11)|((uint32_t)h<<6)|mi;
  uint8_t dayBit=currentWeekdayBit(y,mo,d);
  for(uint8_t i=0;i<ALARM_SLOT_COUNT;i++){
    AlarmSlot &a=alarms[i];
    if(!a.configured || !(a.flags&0x01) || a.hour!=h || a.minute!=mi || a.lastTriggerMinuteKey==minuteKey) continue;
    uint8_t days=a.flags&0xFE;
    if(days && !(days&dayBit)) continue;
    a.lastTriggerMinuteKey=minuteKey;
    if(days==0){ a.flags&=~0x01; saveAlarmMeta(i); } // one-shot
    startAlarm(i); break;
  }
}

bool processAlarmCommand(const uint8_t *data,size_t len){
  if(len<12 || data[2]!=0x00 || data[3]!=0x80) return false;
  uint8_t slot=data[4]; if(slot>=ALARM_SLOT_COUNT){sendCommandAck(0x00,0x80);return true;}

  AlarmSlot previous=alarms[slot];
  AlarmSlot candidate=previous;
  candidate.configured=true; candidate.flags=data[5]; candidate.hour=data[6]; candidate.minute=data[7]; candidate.durationSec=data[8];
  if(candidate.hour>23 || candidate.minute>59){
#if DEBUG_SERIAL
    Serial.println("ALARM ERROR: invalid time");
#endif
    sendCommandAck(0x00,0x80); return true;
  }

  // Short packet: metadata-only update. If the same slot currently has an
  // incomplete media transaction, discard that staging file first.
  if(len<ALARM_HEADER_SIZE){
    if(alarmUpload.active && alarmUpload.slot==slot) abortAlarmUpload("metadata update");
    if(len>9)candidate.reserved1=data[9]; if(len>10)candidate.contentType=data[10]; if(len>11)candidate.buzzer=data[11];
    bool stored=saveAlarmMetaValue(slot,candidate);
    if(stored) alarms[slot]=candidate;
    else saveAlarmMetaValue(slot,previous);
#if DEBUG_SERIAL
    Serial.print("ALARM META slot=");Serial.print(slot);Serial.print(" flags=0x");Serial.print(candidate.flags,HEX);Serial.print(" ");Serial.print(candidate.hour);Serial.print(":");Serial.print(candidate.minute);Serial.print(" stored=");Serial.println(stored?"YES":"NO");
#endif
    sendCommandAck(0x00,0x80); return true;
  }

  candidate.reserved1=data[9]; candidate.contentType=data[10]; candidate.buzzer=data[11]; candidate.reserved2=data[12];
  if(!littleFsReady){
#if DEBUG_SERIAL
    Serial.println("ALARM ERROR: LittleFS unavailable for media update");
#endif
    sendCommandAck(0x00,0x80); return true;
  }
  candidate.mediaSize=(uint32_t)data[13]|((uint32_t)data[14]<<8)|((uint32_t)data[15]<<16)|((uint32_t)data[16]<<24);
  candidate.mediaCRC=(uint32_t)data[17]|((uint32_t)data[18]<<8)|((uint32_t)data[19]<<16)|((uint32_t)data[20]<<24);
  candidate.reserved3=(uint16_t)data[21]|((uint16_t)data[22]<<8); candidate.mediaId=data[23];

  const uint8_t *media=data+ALARM_HEADER_SIZE;
  const size_t chunkSize=len-ALARM_HEADER_SIZE;

  // The observed 64x64 app uses reserved2=0x00 on the first logical packet
  // and 0x02 on the following packet. Treat reserved2 as diagnostic only:
  // completeness is determined from accumulated bytes, declared mediaSize and
  // the CRC of the complete media object.
  bool needBegin=!alarmUpload.active;
  if(alarmUpload.active){
    const bool sameTransaction = alarmUpload.slot==slot &&
      alarmUploadMetadataMatches(alarmUpload.candidate,candidate);

    // A repeated first chunk is most safely interpreted as a restarted app
    // transfer rather than appended as duplicate media.
    if(candidate.reserved2==0x00 && alarmUpload.received>0){
      abortAlarmUpload("transfer restarted");
      needBegin=true;
    } else if(!sameTransaction){
      if(candidate.reserved2==0x00){
        abortAlarmUpload("new transaction");
        needBegin=true;
      } else {
#if DEBUG_SERIAL
        Serial.print("ALARM ERROR: unexpected continuation slot="); Serial.print(slot);
        Serial.print(" part=0x"); Serial.print(candidate.reserved2,HEX);
        Serial.print(" chunk="); Serial.print(chunkSize);
        Serial.print(" media="); Serial.println(candidate.mediaSize);
#endif
        sendCommandAck(0x00,0x80); return true;
      }
    }
  }

  if(needBegin){
    // A partial packet marked as a continuation cannot be reconstructed if its
    // first chunk was not received. A one-packet media update is accepted
    // regardless of the reserved2 value because no continuation is required.
    if(candidate.reserved2!=0x00 && chunkSize<candidate.mediaSize){
#if DEBUG_SERIAL
      Serial.print("ALARM ERROR: continuation without start slot="); Serial.print(slot);
      Serial.print(" part=0x"); Serial.print(candidate.reserved2,HEX);
      Serial.print(" chunk="); Serial.print(chunkSize);
      Serial.print(" media="); Serial.println(candidate.mediaSize);
#endif
      sendCommandAck(0x00,0x80); return true;
    }
    if(!beginAlarmUpload(slot,candidate)){
#if DEBUG_SERIAL
      Serial.print("ALARM ERROR: could not create staging file slot="); Serial.println(slot);
#endif
      sendCommandAck(0x00,0x80); return true;
    }
  }

  if((uint64_t)alarmUpload.received+(uint64_t)chunkSize > (uint64_t)alarmUpload.candidate.mediaSize){
#if DEBUG_SERIAL
    Serial.print("ALARM ERROR: media overflow slot="); Serial.print(slot);
    Serial.print(" received="); Serial.print(alarmUpload.received);
    Serial.print(" chunk="); Serial.print(chunkSize);
    Serial.print(" expected="); Serial.println(alarmUpload.candidate.mediaSize);
#endif
    abortAlarmUpload("media overflow");
    sendCommandAck(0x00,0x80); return true;
  }

  String tmp=alarmTempFileName(slot);
  File f=LittleFS.open(tmp,"a");
  bool writeOK=(bool)f;
  if(writeOK && (uint32_t)f.size()!=alarmUpload.received) writeOK=false;
  if(writeOK && chunkSize && f.write(media,chunkSize)!=chunkSize) writeOK=false;
  if(f)f.close();
  if(!writeOK){
#if DEBUG_SERIAL
    Serial.print("ALARM ERROR: staging write failed slot="); Serial.println(slot);
#endif
    abortAlarmUpload("staging write");
    sendCommandAck(0x00,0x80); return true;
  }

  alarmUpload.runningCRC=crc32Update(alarmUpload.runningCRC,media,chunkSize);
  alarmUpload.received+=(uint32_t)chunkSize;
  alarmUpload.lastChunkAt=millis();
  alarmUpload.candidate.reserved2=candidate.reserved2;

#if DEBUG_SERIAL
  Serial.print("ALARM RX slot="); Serial.print(slot);
  Serial.print(" part=0x"); Serial.print(candidate.reserved2,HEX);
  Serial.print(" chunk="); Serial.print(chunkSize);
  Serial.print(" total="); Serial.print(alarmUpload.received);
  Serial.print("/"); Serial.println(alarmUpload.candidate.mediaSize);
#endif

  if(alarmUpload.received<alarmUpload.candidate.mediaSize){
    sendCommandAck(0x00,0x80); return true;
  }

  const uint32_t calc=alarmUpload.runningCRC^0xFFFFFFFFUL;
  if(calc!=alarmUpload.candidate.mediaCRC){
#if DEBUG_SERIAL
    Serial.print("ALARM CRC ERROR calc=0x");Serial.print(calc,HEX);Serial.print(" expected=0x");Serial.println(alarmUpload.candidate.mediaCRC,HEX);
#endif
    abortAlarmUpload("CRC mismatch");
    sendCommandAck(0x00,0x80); return true;
  }

  AlarmSlot committed=alarmUpload.candidate;
  AlarmSlot rollback=alarmUpload.previous;
  String dst=alarmFileName(slot), bak=alarmBackupFileName(slot);
  LittleFS.remove(bak);
  bool hadOld=LittleFS.exists(dst);
  bool stored=true;
  if(hadOld) stored=LittleFS.rename(dst,bak);
  if(stored) stored=LittleFS.rename(tmp,dst);
  if(!stored){
    LittleFS.remove(tmp);
    if(hadOld && LittleFS.exists(bak) && !LittleFS.exists(dst)) LittleFS.rename(bak,dst);
  } else if(!saveAlarmMetaValue(slot,committed)){
    // Metadata did not commit: roll the filesystem and NVS back to the
    // previous slot so a failed update cannot destroy a working Alarm.
    LittleFS.remove(dst);
    if(hadOld && LittleFS.exists(bak)) LittleFS.rename(bak,dst);
    saveAlarmMetaValue(slot,rollback);
    stored=false;
  } else {
    alarms[slot]=committed;
    LittleFS.remove(bak);
  }

  // The staging path was either renamed or rolled back. Reset the in-memory
  // transaction without deleting the committed file.
  alarmUpload=AlarmUploadState();

#if DEBUG_SERIAL
  Serial.print("ALARM SAVE slot=");Serial.print(slot);Serial.print(" flags=0x");Serial.print(committed.flags,HEX);
  Serial.print(" time=");Serial.print(committed.hour);Serial.print(":");Serial.print(committed.minute);Serial.print(" dur=");Serial.print(committed.durationSec);
  Serial.print(" type=");Serial.print(committed.contentType);Serial.print(" buzzer=");Serial.print(committed.buzzer);Serial.print(" bytes=");Serial.print(committed.mediaSize);
  Serial.print(" mediaId=0x");Serial.print(committed.mediaId,HEX);Serial.print(" stored=");Serial.println(stored?"YES":"NO");
#endif
  // Preserve the compatibility ACK while the original device's error status
  // for Alarm storage failures remains unknown.
  sendCommandAck(0x00,0x80); return true;
}

// ======================================================
// GIF
// ======================================================
AnimatedGIF *gif = nullptr;
bool gifDecoderTeardownPending = false;
bool gifDecoderOpenPending = false;
uint32_t gifDecoderOpenAt = 0;
size_t gifSize = 0;
bool gifStoredOnFS = false;
bool gifEventPlaybackFileActive = false;
// B73: RX and playback files are deliberately different.  BLE writes happen
// on Core 0 while AnimatedGIF playback runs from loop() on Core 1; touching
// the file/decoder currently in use from the BLE callback can corrupt the heap.
const char *GIF_PLAY_FILE = "/gif_play.gif";
const char *GIF_PLAY_BACKUP_FILE = "/gif_play.bak";
const char *GIF_RX_FILES[2] = { "/gif_rx0.tmp", "/gif_rx1.tmp" };
// Alarm/Schedule GIFs use a disposable PLAY copy. Their persistent source
// files remain free to participate in staging/backup/rollback transactions.
const char *EVENT_GIF_PLAY_FILE = "/event_play.gif";

uint32_t storedFileSize(const char *path) {
  if(!littleFsReady) return 0;
  File f=LittleFS.open(path,"r");
  if(!f) return 0;
  uint32_t size=(uint32_t)f.size();
  f.close();
  return size;
}

void recoverGifPlayFile() {
  if(!littleFsReady) return;
  bool havePlay=LittleFS.exists(GIF_PLAY_FILE);
  bool haveBackup=LittleFS.exists(GIF_PLAY_BACKUP_FILE);
  if(havePlay) LittleFS.remove(GIF_PLAY_BACKUP_FILE);
  else if(haveBackup) LittleFS.rename(GIF_PLAY_BACKUP_FILE,GIF_PLAY_FILE);
  // RX files belong to an interrupted BLE session and are never resumable.
  LittleFS.remove(GIF_RX_FILES[0]);
  LittleFS.remove(GIF_RX_FILES[1]);
  // Event PLAY is an ephemeral copy and is never authoritative after reboot.
  LittleFS.remove(EVENT_GIF_PLAY_FILE);
}

bool promoteGifRxToPlay(uint8_t slot) {
  if(!littleFsReady || slot>1) return false;
  const char *rxPath=GIF_RX_FILES[slot];
  if(!LittleFS.exists(rxPath)) return false;

  // Reconcile any leftover backup from an earlier interrupted promotion before
  // starting a new replacement; never discard the only known-good PLAY file.
  if(!LittleFS.exists(GIF_PLAY_FILE) && LittleFS.exists(GIF_PLAY_BACKUP_FILE)) {
    if(!LittleFS.rename(GIF_PLAY_BACKUP_FILE,GIF_PLAY_FILE)) return false;
  } else if(LittleFS.exists(GIF_PLAY_FILE) && LittleFS.exists(GIF_PLAY_BACKUP_FILE)) {
    LittleFS.remove(GIF_PLAY_BACKUP_FILE);
  }

  bool hadOld=LittleFS.exists(GIF_PLAY_FILE);
  if(hadOld && !LittleFS.rename(GIF_PLAY_FILE,GIF_PLAY_BACKUP_FILE)) return false;

  if(!LittleFS.rename(rxPath,GIF_PLAY_FILE)){
    if(hadOld && LittleFS.exists(GIF_PLAY_BACKUP_FILE))
      LittleFS.rename(GIF_PLAY_BACKUP_FILE,GIF_PLAY_FILE);
    return false;
  }

  LittleFS.remove(GIF_PLAY_BACKUP_FILE);
  return true;
}

File gifBulkFile;
size_t gifRxWriteOffset = 0;
uint8_t nextGifRxSlot = 0;
bool gifLoaded = false;
bool gifPlaying = false;
bool gifRestartPending = false;
bool gifFrameWasDrawn = false;
// A completed BLE GIF transfer is acknowledged immediately and opened later
// from loop().  AnimatedGIF/LittleFS initialization must not run inside the
// BLE write callback: complex 64x64 GIFs can otherwise corrupt/crash Core 0.
bool pendingGifStart = false;
int8_t pendingGifSlot = -1;
uint32_t pendingGifSize = 0;
uint32_t pendingGifStartAt = 0;
uint32_t gifNextFrameAt = 0;
CRGB *gifFrame = nullptr;

// ======================================================
// DEVICE-ASSET CAROUSEL
// BUILD 97 consolidates the hardware-tested 12-slot Device Assets model. The official
// app may keep multiple 12-slot pages in its UI, but a pushed page addresses
// device slots 0..11. Command 02/01 carries a slot-setup descriptor; current
// official-app captures use count=12 followed by 0..11 even when only a subset
// of those slots receives a GIF. Bulk bytes 13-14 are the per-slot dwell
// (timeSign, LE seconds) and byte 15 is the device asset slot/imageIndex.
// Command 0A/01 enters/starts the asset view, but current official-app captures
// show it may arrive before a later page push and is not necessarily repeated
// afterwards. BUILD 95 established Assets-view intent across 02/01,
// stores GIF and observed TEXT slots without rendering them during the push,
// then starts the completed bank after a short upload-idle settle interval.
// Empty positions are skipped.
// ======================================================
struct __attribute__((packed)) CarouselSlotMeta {
  uint8_t configured = 0;
  uint8_t dataType = 0;       // 1=GIF, 3=TEXT (observed in Device Assets)
  uint16_t dwellSeconds = 0;
  uint32_t mediaSize = 0;
  uint32_t mediaCRC = 0;
};

CarouselSlotMeta carouselSlots[CAROUSEL_SLOT_COUNT];
Preferences carouselPrefs;
bool carouselPrefsReady = false;
uint8_t carouselOrder[CAROUSEL_SLOT_COUNT] = {};
uint8_t carouselOrderCount = 0;
bool carouselUploadOpen = false;
// Emulator UX policy: while a Device Assets bank is being replaced, force the
// physical matrix black without changing the protocol-controlled screenOn state.
bool carouselUploadBlackout = false;

// Procedural Carousel transfer indicator. The app does not announce the byte
// sizes of future assets, so whole-Carousel progress is asset-weighted while
// the active asset uses its real byte fraction.
bool carouselTransferIndicatorActive = false;
bool carouselTransferIndicatorVisible = false;
uint32_t carouselTransferIndicatorStartedAt = 0;
uint32_t carouselTransferIndicatorLastFrameAt = 0;
uint32_t carouselTransferExpectedBytes = 0;
uint32_t carouselTransferReceivedBytes = 0;
uint16_t carouselUploadCompletedMask = 0;
constexpr uint32_t CAROUSEL_TRANSFER_INDICATOR_DELAY_MS = 250UL;
bool carouselEnterRequested = false;
bool carouselStartPending = false;
bool carouselActive = false;
int8_t carouselActiveSlot = -1;
uint32_t carouselSlotStartedAt = 0;
uint32_t carouselLastAssetCommitAt = 0;
bool gifCarouselPlaybackFileActive = false;

// ======================================================
// PRESET / DEFAULT (volatile mini-playlist)
// Hardware captures from the official app use device slots 14..19 and command
// 06/02 <count> <slot...>. The app re-sends all media when a preset is invoked.
// Captured assets use timeSign=5 while the original display visibly advances
// at about 3 seconds per item, so timeSign is retained as opaque metadata and
// is NOT interpreted as seconds here. Preset media is intentionally volatile.
// ======================================================
struct PresetSlotMeta {
  uint8_t configured = 0;
  uint8_t dataType = 0;       // 1=GIF/image, 3=TEXT
  uint16_t timeSign = 0;      // observed 5; semantics not established
  uint32_t mediaSize = 0;
  uint32_t mediaCRC = 0;
};

PresetSlotMeta presetSlots[PRESET_SLOT_COUNT];
uint8_t presetOrder[PRESET_SLOT_COUNT] = {}; // local slots 0..5
uint8_t presetOrderCount = 0;
bool presetActive = false;
int8_t presetActiveSlot = -1;
uint32_t presetSlotStartedAt = 0;
uint32_t presetSlotHoldMs = PRESET_DWELL_MS;
bool gifPresetPlaybackFileActive = false;

// ======================================================
// PACKET / BULK
// ======================================================
uint8_t packetBuffer[MAX_PACKET_SIZE];
size_t packetReceived = 0;
size_t packetExpected = 0;
uint32_t packetLastRxMs = 0;
uint32_t bulkLastRxMs = 0;

struct BulkTransferState {
  bool active = false;
  uint8_t dataType = 0;
  uint32_t expectedSize = 0;
  uint32_t receivedSize = 0;
  uint32_t expectedCRC = 0;
  uint32_t runningCRC = 0xFFFFFFFF;
  uint32_t chunkCount = 0;
  bool gifToFS = false;
  bool carouselToFS = false;
  bool presetToFS = false;
  int8_t gifRxSlot = -1;
  int8_t carouselLocalSlot = -1;
  int8_t presetLocalSlot = -1;
  uint16_t timeSign = 0;
  uint8_t imageIndex = 0xFF;
  String format = "UNKNOWN";
} bulk;

uint8_t textPayload[MAX_TEXT_PAYLOAD];
size_t textPayloadReceived = 0;

// Local TEXT viewport state. The app already sends the complete glyph stream;
// PIN/UP/DOWN page through it when the full line does not fit the matrix.
uint8_t textFirstVisibleGlyph = 0;
uint32_t textPageChangedAt = 0;

uint8_t *rawRgbData = nullptr;
size_t rawRgbWriteOffset = 0;

bool pendingDeviceInfoPush = false;
uint32_t deviceInfoPushAt = 0;
bool pendingSoftReset = false;
uint32_t softResetAt = 0;
bool pendingAdvertisingRestart = false;
uint32_t advertisingRestartAt = 0;

// ======================================================
// UTILITY
// ======================================================
void dumpHex(const uint8_t *data, size_t len) {
#if DEBUG_SERIAL
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < len) Serial.print(' ');
  }
  Serial.println();
#endif
}

void reportHeap(const char *label) {
#if DEBUG_SERIAL
  Serial.print("[HEAP] "); Serial.print(label);
  Serial.print(": free="); Serial.print(ESP.getFreeHeap());
  Serial.print(" min="); Serial.print(ESP.getMinFreeHeap());
  Serial.print(" largest=");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
}

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
  }
  return crc;
}

bool fileMatchesMedia(const String &path, uint32_t expectedSize, uint32_t expectedCRC) {
  if(!littleFsReady) return false;
  File f=LittleFS.open(path,"r");
  if(!f || (uint32_t)f.size()!=expectedSize){ if(f)f.close(); return false; }
  uint8_t buf[256];
  uint32_t crc=0xFFFFFFFF;
  uint32_t remaining=expectedSize;
  while(remaining){
    size_t want=remaining>sizeof(buf)?sizeof(buf):(size_t)remaining;
    size_t got=f.read(buf,want);
    if(got!=want){f.close();return false;}
    crc=crc32Update(crc,buf,got);
    remaining-=(uint32_t)got;
  }
  f.close();
  return (crc^0xFFFFFFFF)==expectedCRC;
}

uint16_t logicalIndex(uint8_t x, uint8_t y) {
  return (uint16_t)y * MATRIX_WIDTH + x;
}

uint16_t physicalXY(uint8_t x, uint8_t y) {
  if ((y & 1) == 0) return (uint16_t)y * PHYSICAL_MATRIX_WIDTH + x;
  return (uint16_t)y * PHYSICAL_MATRIX_WIDTH + PHYSICAL_MATRIX_WIDTH - 1 - x;
}

// Small 3x5 diagnostic font used only by the temporary release/build overlay.
// Bits are stored row-major, three bits per row, LSB = leftmost pixel.
static uint16_t versionGlyphBits(char c) {
#define VG(r0,r1,r2,r3,r4) ((uint16_t)(r0) | ((uint16_t)(r1)<<3) | ((uint16_t)(r2)<<6) | ((uint16_t)(r3)<<9) | ((uint16_t)(r4)<<12))
  switch (c) {
    case '0': return VG(0x7,0x5,0x5,0x5,0x7);
    case '1': return VG(0x2,0x6,0x2,0x2,0x7);
    case '2': return VG(0x7,0x1,0x7,0x4,0x7);
    case '3': return VG(0x7,0x1,0x7,0x1,0x7);
    case '4': return VG(0x5,0x5,0x7,0x1,0x1);
    case '5': return VG(0x7,0x4,0x7,0x1,0x7);
    case '6': return VG(0x7,0x4,0x7,0x5,0x7);
    case '7': return VG(0x7,0x1,0x2,0x2,0x2);
    case '8': return VG(0x7,0x5,0x7,0x5,0x7);
    case '9': return VG(0x7,0x5,0x7,0x1,0x7);
    case 'B': return VG(0x6,0x5,0x6,0x5,0x6);
    case 'R': return VG(0x6,0x5,0x6,0x5,0x5);
    case '.': return VG(0x0,0x0,0x0,0x0,0x2);
    default:  return 0;
  }
#undef VG
}

static bool versionLinePixel(const char *text, uint16_t x, uint16_t y,
                             uint16_t originX, uint16_t originY, uint8_t scale) {
  if (!text || !scale || x < originX || y < originY) return false;
  const uint16_t ux=(x-originX)/scale;
  const uint16_t uy=(y-originY)/scale;
  if (uy>=5) return false;
  const size_t len=strlen(text);
  const uint16_t textUnits=len ? (uint16_t)(len*4U-1U) : 0U;
  if (ux>=textUnits) return false;
  const uint16_t ci=ux/4U;
  const uint8_t col=(uint8_t)(ux%4U);
  if (ci>=len || col>=3) return false;
  const uint16_t bits=versionGlyphBits(text[ci]);
  return (bits & ((uint16_t)1U << (uy*3U+col))) != 0;
}

static CRGB versionSplashPhysicalPixel(uint16_t px, uint16_t py) {
  static const char line1[] = "R" IDOT_STRINGIFY(FW_RELEASE_MAJOR) "." IDOT_STRINGIFY(FW_RELEASE_MINOR);
  static const char line2[] = "B" IDOT_STRINGIFY(FW_BUILD);
  const uint16_t width1=(uint16_t)(strlen(line1)*4U-1U);
  const uint16_t width2=(uint16_t)(strlen(line2)*4U-1U);
  const uint16_t widthUnits=max(width1,width2);
  const uint16_t heightUnits=12U; // 5 rows + 2-row gap + 5 rows.
  uint8_t scale=(uint8_t)min((uint16_t)(PHYSICAL_MATRIX_WIDTH/widthUnits),
                             (uint16_t)(PHYSICAL_MATRIX_HEIGHT/heightUnits));
  if (scale<1) scale=1;
  if (scale>4) scale=4;
  const uint16_t totalHeight=heightUnits*scale;
  const uint16_t y0=(PHYSICAL_MATRIX_HEIGHT>totalHeight) ? (PHYSICAL_MATRIX_HEIGHT-totalHeight)/2U : 0U;
  const uint16_t w1=width1*scale, w2=width2*scale;
  const uint16_t x1=(PHYSICAL_MATRIX_WIDTH>w1) ? (PHYSICAL_MATRIX_WIDTH-w1)/2U : 0U;
  const uint16_t x2=(PHYSICAL_MATRIX_WIDTH>w2) ? (PHYSICAL_MATRIX_WIDTH-w2)/2U : 0U;
  if (versionLinePixel(line1,px,py,x1,y0,scale)) return CRGB(80,180,255);
  if (versionLinePixel(line2,px,py,x2,(uint16_t)(y0+7U*scale),scale)) return CRGB(255,190,40);
  return CRGB::Black;
}

// Scale the final logical framebuffer to one physical pixel. Upscaling uses
// nearest-neighbour replication; downscaling uses a box average, matching the
// independently hardware-tested WLED Usermod policy.
CRGB logicalToPhysicalPixel(uint16_t px, uint16_t py) {
#if APP_POWER_VERSION_SPLASH
  if (versionSplashActive) return versionSplashPhysicalPixel(px,py);
#endif
  const uint16_t sourceWidth = MATRIX_WIDTH;
  const uint16_t sourceHeight = MATRIX_HEIGHT;
  const uint16_t targetWidth = PHYSICAL_MATRIX_WIDTH;
  const uint16_t targetHeight = PHYSICAL_MATRIX_HEIGHT;

  if (sourceWidth == targetWidth && sourceHeight == targetHeight)
    return framebuffer[logicalIndex((uint8_t)px, (uint8_t)py)];

  const bool downscale = targetWidth < sourceWidth || targetHeight < sourceHeight;
  if (!downscale) {
    const uint16_t sx = (uint32_t)px * sourceWidth / targetWidth;
    const uint16_t sy = (uint32_t)py * sourceHeight / targetHeight;
    return framebuffer[logicalIndex((uint8_t)sx, (uint8_t)sy)];
  }

  const uint16_t sy0 = (uint32_t)py * sourceHeight / targetHeight;
  uint16_t sy1 = (uint32_t)(py + 1u) * sourceHeight / targetHeight;
  if (sy1 <= sy0) sy1 = sy0 + 1u;
  if (sy1 > sourceHeight) sy1 = sourceHeight;

  const uint16_t sx0 = (uint32_t)px * sourceWidth / targetWidth;
  uint16_t sx1 = (uint32_t)(px + 1u) * sourceWidth / targetWidth;
  if (sx1 <= sx0) sx1 = sx0 + 1u;
  if (sx1 > sourceWidth) sx1 = sourceWidth;

  uint32_t r=0, g=0, b=0, samples=0;
  for (uint16_t sy=sy0; sy<sy1; ++sy) {
    for (uint16_t sx=sx0; sx<sx1; ++sx) {
      const CRGB &c=framebuffer[logicalIndex((uint8_t)sx,(uint8_t)sy)];
      r += c.r; g += c.g; b += c.b; ++samples;
    }
  }
  if (!samples) samples=1;
  return CRGB((uint8_t)(r/samples),(uint8_t)(g/samples),(uint8_t)(b/samples));
}

void clearFramebuffer(const CRGB &color) {
  fill_solid(framebuffer, NUM_LEDS, color);
}

void putPixel(int16_t x, int16_t y, const CRGB &c) {
  if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return;
  framebuffer[logicalIndex((uint8_t)x, (uint8_t)y)] = c;
}

// Several hand-tuned clock/audio/scoreboard renderers were reconstructed on a
// 16x16 reference canvas. Preserve their appearance on larger logical panels by
// scaling that 16x16 artwork after it has been rendered. App-supplied media and
// graffiti bypass this helper and therefore remain truly native-resolution.
void scaleLegacy16CanvasToLogical() {
  if (MATRIX_WIDTH == 16 && MATRIX_HEIGHT == 16) return;
  CRGB base[16 * 16];
  for (uint8_t y=0; y<16; y++)
    for (uint8_t x=0; x<16; x++)
      base[(uint16_t)y*16+x] = (x<MATRIX_WIDTH && y<MATRIX_HEIGHT) ? framebuffer[logicalIndex(x,y)] : CRGB::Black;

  for (uint8_t y=0; y<MATRIX_HEIGHT; y++) {
    const uint8_t sy=(uint16_t)y*16U/MATRIX_HEIGHT;
    for (uint8_t x=0; x<MATRIX_WIDTH; x++) {
      const uint8_t sx=(uint16_t)x*16U/MATRIX_WIDTH;
      framebuffer[logicalIndex(x,y)] = base[(uint16_t)sy*16+sx];
    }
  }
}

void setStatusLed(bool on) {
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

void getCurrentTime(uint8_t &h, uint8_t &m, uint8_t &s) {
#if RTC_ENABLED
  if (rtcReady && rtcTimeValid) {
    DateTime now = rtc.now();
    h = now.hour(); m = now.minute(); s = now.second();
    return;
  }
#endif
  if (!clockSynced) { h = m = s = 0; return; }
  uint32_t elapsed = (millis() - syncMillis) / 1000UL;
  uint32_t total = ((uint32_t)syncHour * 3600UL + (uint32_t)syncMinute * 60UL + syncSecond + elapsed) % 86400UL;
  h = total / 3600UL;
  total %= 3600UL;
  m = total / 60UL;
  s = total % 60UL;
}

bool isEnergySavingActive() {
  if (!energySaving.enabled) return false;
  uint8_t h=0, m=0, s=0;
#if RTC_ENABLED
  if (rtcReady && rtcTimeValid) {
    DateTime now=rtc.now();
    h=now.hour(); m=now.minute(); s=now.second();
  } else if (clockSynced) {
    getCurrentTime(h, m, s);
  } else return false;
#else
  if (!clockSynced) return false;
  getCurrentTime(h, m, s);
#endif
  uint16_t now = (uint16_t)h * 60 + m;
  uint16_t start = (uint16_t)energySaving.startHour * 60 + energySaving.startMinute;
  uint16_t end = (uint16_t)energySaving.endHour * 60 + energySaving.endMinute;
  if (start == end) return false;
  return (start < end) ? (now >= start && now < end) : (now >= start || now < end);
}

uint8_t effectiveBrightnessPercent() {
  uint8_t v = brightnessPercent;
  if (isEnergySavingActive()) {
    uint8_t r = min((uint8_t)100, energySaving.reductionPercent);
    v = ((uint16_t)v * (100 - r)) / 100;
  }
  return v;
}

uint8_t brightnessToOutput(uint8_t percent) {
  percent = min((uint8_t)100, percent);
  return ((uint16_t)percent * MAX_LED_BRIGHTNESS) / 100;
}

CRGB scaleOutputBrightness(const CRGB &input, uint8_t level) {
  return CRGB(
    (uint8_t)(((uint16_t)input.r * level) / 255U),
    (uint8_t)(((uint16_t)input.g * level) / 255U),
    (uint8_t)(((uint16_t)input.b * level) / 255U)
  );
}

void clearPhysicalDisplay() {
#if DISPLAY_BACKEND == DISPLAY_BACKEND_WS2812
  FastLED.clear();
  FastLED.show();
#elif DISPLAY_BACKEND == DISPLAY_BACKEND_HUB75
  if (hub75Matrix) hub75Matrix->clearScreen();
#endif
}

void refreshMatrix() {
  if (!screenOn || (carouselUploadBlackout && !carouselTransferIndicatorVisible)) {
    clearPhysicalDisplay();
    lastAppliedOutputBrightness = 0xFF;
    return;
  }

  const uint8_t outputBrightness=brightnessToOutput(effectiveBrightnessPercent());

#if DISPLAY_BACKEND == DISPLAY_BACKEND_WS2812
  FastLED.clear();
  for (uint16_t py=0; py<PHYSICAL_MATRIX_HEIGHT; ++py) {
    for (uint16_t px=0; px<PHYSICAL_MATRIX_WIDTH; ++px) {
      uint16_t outX=px, outY=py;
#if MATRIX_MIRROR_X
      outX=PHYSICAL_MATRIX_WIDTH-1-outX;
#endif
      if (flipped180) {
        outX=PHYSICAL_MATRIX_WIDTH-1-outX;
        outY=PHYSICAL_MATRIX_HEIGHT-1-outY;
      }
      leds[physicalXY((uint8_t)outX,(uint8_t)outY)] = logicalToPhysicalPixel(px,py);
    }
  }
  FastLED.setBrightness(outputBrightness);
  FastLED.show();
#elif DISPLAY_BACKEND == DISPLAY_BACKEND_HUB75
  if (!hub75Matrix) return;
  // MatrixPanel_I2S_DMA controls panel brightness through the output-enable
  // timing. Do not scale RGB channels here: doing so destroys gradient
  // resolution before the driver's 8-bit bitplanes are generated.
  if (outputBrightness != lastAppliedOutputBrightness)
    hub75Matrix->setBrightness8(outputBrightness);
  for (uint16_t py=0; py<PHYSICAL_MATRIX_HEIGHT; ++py) {
    for (uint16_t px=0; px<PHYSICAL_MATRIX_WIDTH; ++px) {
      uint16_t outX=px, outY=py;
      if (flipped180) {
        outX=PHYSICAL_MATRIX_WIDTH-1-outX;
        outY=PHYSICAL_MATRIX_HEIGHT-1-outY;
      }
      const CRGB c=logicalToPhysicalPixel(px,py);
      hub75Matrix->drawPixelRGB888(outX,outY,c.r,c.g,c.b);
    }
  }
#else
  #error "Unsupported DISPLAY_BACKEND"
#endif

  lastAppliedOutputBrightness=outputBrightness;
}

void updateEnergySavingOutput(uint32_t now) {
  // ECO is a local emulator policy inferred from the app command. Check once
  // per second so static images also change brightness at interval boundaries.
  if ((uint32_t)(now-lastEnergySavingCheckMs) < 1000UL) return;
  lastEnergySavingCheckMs=now;
  if (!screenOn) return;
  uint8_t wanted=brightnessToOutput(effectiveBrightnessPercent());
  if (wanted != lastAppliedOutputBrightness) refreshMatrix();
}

// ======================================================
// CAROUSEL TRANSFER INDICATOR
// ======================================================
uint8_t carouselCompletedUploadCountExcluding(uint8_t activeSlot) {
  uint16_t mask=carouselUploadCompletedMask;
  if(activeSlot<CAROUSEL_SLOT_COUNT) mask &= (uint16_t)~(uint16_t(1U)<<activeSlot);
  uint8_t count=0;
  for(uint8_t i=0;i<CAROUSEL_SLOT_COUNT;i++) if(mask&(uint16_t(1U)<<i)) count++;
  return count;
}

void beginCarouselTransferIndicator(uint32_t totalBytes, uint8_t activeSlot) {
  const bool first=!carouselTransferIndicatorActive;
  carouselTransferIndicatorActive=true;
  if(first){
    carouselTransferIndicatorStartedAt=millis();
    carouselTransferIndicatorLastFrameAt=0;
    carouselTransferIndicatorVisible=false;
    clearFramebuffer();
    clearPhysicalDisplay();
  }
  carouselTransferExpectedBytes=totalBytes;
  carouselTransferReceivedBytes=0;
  (void)activeSlot;
}

void updateCarouselTransferIndicatorProgress(uint32_t receivedBytes, uint32_t totalBytes) {
  if(!carouselTransferIndicatorActive) return;
  carouselTransferReceivedBytes=receivedBytes;
  if(totalBytes) carouselTransferExpectedBytes=totalBytes;
}

void endCarouselTransferIndicator() {
  carouselTransferIndicatorActive=false;
  carouselTransferIndicatorVisible=false;
  carouselTransferExpectedBytes=0;
  carouselTransferReceivedBytes=0;
  carouselTransferIndicatorLastFrameAt=0;
}

void renderCarouselTransferIndicator(uint32_t now) {
  if(!carouselTransferIndicatorActive) return;
  if((uint32_t)(now-carouselTransferIndicatorStartedAt)<CAROUSEL_TRANSFER_INDICATOR_DELAY_MS){
    carouselTransferIndicatorVisible=false;
    return;
  }
  if(carouselTransferIndicatorLastFrameAt &&
     (uint32_t)(now-carouselTransferIndicatorLastFrameAt)<50UL) return;
  carouselTransferIndicatorLastFrameAt=now;
  carouselTransferIndicatorVisible=true;
  clearFramebuffer();

  const uint8_t w=MATRIX_WIDTH, h=MATRIX_HEIGHT;
  const uint8_t unit=w>=64 ? 3U : (w>=32 ? 2U : 1U);
  const int16_t cx=(int16_t)(w/2U);
  const int16_t iconW=(int16_t)(10U*unit);
  const int16_t iconH=(int16_t)(6U*unit);
  const int16_t iconX=cx-iconW/2;
  const int16_t iconY=(int16_t)h-iconH-(int16_t)(3U*unit);

  auto px=[&](int16_t x,int16_t y,const CRGB &c){
    if(x>=0&&y>=0&&x<w&&y<h) putPixel((uint8_t)x,(uint8_t)y,c);
  };
  auto block=[&](int16_t x,int16_t y,uint8_t bw,uint8_t bh,const CRGB &c){
    for(uint8_t yy=0;yy<bh;yy++) for(uint8_t xx=0;xx<bw;xx++) px(x+xx,y+yy,c);
  };

  const CRGB outline(55,160,235), cells(80,215,255);
  for(int16_t x=0;x<iconW;x++){
    block(iconX+x,iconY,1,unit,outline);
    block(iconX+x,iconY+iconH-unit,1,unit,outline);
  }
  for(int16_t y=0;y<iconH;y++){
    block(iconX,iconY+y,unit,1,outline);
    block(iconX+iconW-unit,iconY+y,unit,1,outline);
  }
  for(uint8_t iy=0;iy<2;iy++) for(uint8_t ix=0;ix<4;ix++)
    block(iconX+(int16_t)((2+ix*2)*unit),iconY+(int16_t)((2+iy*2)*unit),unit,unit,cells);

  const uint8_t phase=(uint8_t)((now/120U)%4U);
  const int16_t packetSize=(int16_t)(2U*unit);
  const int16_t packetTop=iconY-(int16_t)((6U-phase)*unit);
  const int16_t packetX=cx-packetSize/2;
  block(packetX,packetTop,(uint8_t)packetSize,(uint8_t)packetSize,CRGB(245,245,255));
  if(packetSize>=3) block(packetX+unit/2U,packetTop+unit/2U,unit,unit,cells);
  px(cx,packetTop-(int16_t)(2U*unit),CRGB(60,120,155));
  px(cx,packetTop-(int16_t)(4U*unit),CRGB(30,65,85));

  const int16_t barX=(int16_t)(2U*unit);
  const int16_t barY=(int16_t)h-(int16_t)unit;
  const int16_t barW=(int16_t)w-(int16_t)(4U*unit);
  if(barW>0){
    for(int16_t x=0;x<barW;x++) px(barX+x,barY,CRGB(18,42,55));
    int16_t filled=0;
    const uint8_t totalUnits=carouselOrderCount;
    const uint8_t activeSlot=(bulk.carouselLocalSlot>=0)?(uint8_t)bulk.carouselLocalSlot:0xFF;
    const uint8_t done=carouselCompletedUploadCountExcluding(activeSlot);
    if(totalUnits&&carouselTransferExpectedBytes){
      const uint32_t current=carouselTransferReceivedBytes>carouselTransferExpectedBytes ? carouselTransferExpectedBytes : carouselTransferReceivedBytes;
      const uint64_t numerator=(uint64_t(done)*carouselTransferExpectedBytes+current)*(uint64_t)barW;
      const uint64_t denominator=(uint64_t)totalUnits*carouselTransferExpectedBytes;
      filled=denominator?(int16_t)(numerator/denominator):0;
    }else if(carouselTransferExpectedBytes){
      const uint32_t current=carouselTransferReceivedBytes>carouselTransferExpectedBytes ? carouselTransferExpectedBytes : carouselTransferReceivedBytes;
      filled=(int16_t)(((uint64_t)current*(uint64_t)barW)/carouselTransferExpectedBytes);
    }
    if(filled>barW) filled=barW;
    for(int16_t x=0;x<filled;x++) px(barX+x,barY,CRGB(90,230,170));
  }
  refreshMatrix();
}

// ======================================================
// BLE TX
// ======================================================
void sendFA03(const uint8_t *data, size_t len) {
  if (!deviceConnected || !fa03) return;
  fa03->setValue(data, len);
  fa03->notify();
#if DEBUG_SERIAL && DEBUG_SERIAL_VERBOSE
  Serial.print("TX FA03 ["); Serial.print(len); Serial.print("]: "); dumpHex(data, len);
#endif
}

void sendCommandStatus(uint8_t cmd, uint8_t sub, uint8_t status) {
  uint8_t r[] = {0x05,0x00,cmd,sub,status};
  sendFA03(r, sizeof(r));
}
void sendCommandAck(uint8_t cmd, uint8_t sub) { sendCommandStatus(cmd, sub, 0x01); }
void sendTransferAck(uint8_t type, uint8_t status) { sendCommandStatus(type, 0x00, status); }

void sendDeviceInfo() {
  uint8_t r[] = {0x09,0x00,0x01,0x80,FW_RELEASE_MAJOR,FW_RELEASE_MINOR,0x01,IDOTMATRIX_SCREEN_TYPE,0x00};
#if DEVICE_INFO_PROTOCOL_DEBUG
  Serial.print("DEVICE INFO TX: release="); Serial.print(FW_RELEASE);
  Serial.print(" build="); Serial.print(FW_BUILD);
  Serial.print(" screenType="); Serial.print(IDOTMATRIX_SCREEN_TYPE);
  Serial.print(" raw="); dumpHex(r, sizeof(r));
  Serial.println("DEVICE INFO NOTE: app MCU field is encoded by the 9-byte FA03 Device Info response");
#endif
  sendFA03(r, sizeof(r));
}

// ======================================================
// DISPLAY MODE
// ======================================================
void stopGIFPlayback();
void switchDisplayMode(DisplayMode m) {
  if (presetActive) stopPresetPlayback();
  if (displayMode == DISPLAY_GIF && m != DISPLAY_GIF) stopGIFPlayback();
  if (m != DISPLAY_GIF) {
    carouselActive = false;
    carouselActiveSlot = -1;
    carouselStartPending = false;
    carouselEnterRequested = false;
    carouselUploadOpen = false;
    carouselUploadBlackout = false;
    endCarouselTransferIndicator();
    gifCarouselPlaybackFileActive = false;
  }
  if (m == DISPLAY_CLOCK && displayMode != DISPLAY_CLOCK) clockCycleStartedAt = millis();
  displayMode = m;
}

// ======================================================
// FONT 3x5
// ======================================================
const uint8_t digits3x5[10][5] = {
  {0b111,0b101,0b101,0b101,0b111},
  {0b010,0b110,0b010,0b010,0b111},
  {0b111,0b001,0b111,0b100,0b111},
  {0b111,0b001,0b111,0b001,0b111},
  {0b101,0b101,0b111,0b001,0b001},
  {0b111,0b100,0b111,0b001,0b111},
  {0b111,0b100,0b111,0b101,0b111},
  {0b111,0b001,0b010,0b010,0b010},
  {0b111,0b101,0b111,0b101,0b111},
  {0b111,0b101,0b111,0b001,0b111}
};

void drawDigit3x5(uint8_t d, int16_t x, int16_t y, const CRGB &c) {
  if (d > 9) return;
  for (uint8_t row = 0; row < 5; row++)
    for (uint8_t col = 0; col < 3; col++)
      if (digits3x5[d][row] & (1 << (2-col))) putPixel(x+col, y+row, c);
}

bool clockRenderingDate = false;
bool clockColonVisible = true;

void drawColon(int16_t x, int16_t y, const CRGB &c) {
  // Preserve the selected clock renderer/effect exactly. During the 5-second
  // date phase only the separator changes from ':' to '/'.
  if (clockRenderingDate) {
    putPixel(x+1,y,c);
    putPixel(x+1,y+1,c);
    putPixel(x,y+2,c);
    putPixel(x,y+3,c);
    return;
  }
  if (!clockColonVisible) return;
  putPixel(x,y+1,c);
  putPixel(x,y+3,c);
}

void drawTimeTwoRows(uint8_t h, uint8_t m, const CRGB &hc, const CRGB &mc, bool leftColon=false, int8_t xShift=0, int8_t colonShift=0) {
  drawDigit3x5(h/10, 4 + xShift, 2, hc);
  drawDigit3x5(h%10, 8 + xShift, 2, hc);
  drawDigit3x5(m/10, 4 + xShift, 9, mc);
  drawDigit3x5(m%10, 8 + xShift, 9, mc);
  // For the date keep '/' exactly where it was; for time move ':' with the digits.
  drawColon((leftColon ? 2 : 12) + (clockRenderingDate ? 0 : xShift + colonShift), 9, mc);
}

// ======================================================
// CLOCK STYLES - reconstructed from official-app visuals and hardware comparison.
// 0 rainbow frame + digits in the selected color
// 1 Christmas: red digits + green tree
// 2 racing/checker: cyan/magenta frame, orange digits
// 3 selected-color background, black digits
// 4 hourglass: selected-color digits + orange/white hourglass
// 5 cyan/blue frame + orange digits
// 6 cyan/blue frame + digits in the selected color
// 7 RGBY quadrant frame + digits in the selected color
// ======================================================
void drawRainbowBorder() {
  uint8_t hue = (millis()/20) & 0xFF;
  for (uint8_t x=0; x<16; x++) {
    putPixel(x,0,CHSV(hue+x*10,255,255));
    putPixel(15-x,15,CHSV(hue+160+x*10,255,255));
  }
  for (uint8_t y=1; y<15; y++) {
    putPixel(0,y,CHSV(hue+40+y*10,255,255));
    putPixel(15,15-y,CHSV(hue+200+y*10,255,255));
  }
}

void drawChristmasTree() {
  // 6x7 tree in the lower-left area, matching the observed layout.
  CRGB green(0,180,20), darkGreen(0,100,0), yellow(255,210,0), red(255,0,0), magenta(255,0,150);
  putPixel(2,7,yellow);
  putPixel(1,8,green); putPixel(2,8,green); putPixel(3,8,green);
  for (int x=1;x<=3;x++) putPixel(x,9,green);
  for (int x=0;x<=4;x++) putPixel(x,10,green);
  for (int x=0;x<=4;x++) putPixel(x,11,green);
  for (int x=0;x<=5;x++) putPixel(x,12,green);
  for (int x=0;x<=5;x++) putPixel(x,13,darkGreen);
  putPixel(2,14,CRGB(90,45,0)); putPixel(3,14,CRGB(90,45,0));
  putPixel(1,10,red); putPixel(3,11,magenta); putPixel(2,12,yellow); putPixel(4,12,red);
}

void drawCheckerRows() {
  // Observed style: three solid horizontal bands at the top and bottom.
  // Outer = cyan, middle = violet, inner = fuchsia.
  // The borders are not segmented/checkered: each row is continuous.
  const CRGB cyan(0,255,255);
  const CRGB violet(145,0,255);
  const CRGB fuchsia(255,0,170);

  for (uint8_t x=0; x<16; x++) {
    // top
    putPixel(x,0,cyan);
    putPixel(x,1,violet);
    putPixel(x,2,fuchsia);

    // mirrored bottom
    putPixel(x,13,fuchsia);
    putPixel(x,14,violet);
    putPixel(x,15,cyan);
  }
}

void drawFrameStyle5(bool cornerBlocks) {
  CRGB cyan(0,255,255), blue(0,70,255);

  if (cornerBlocks) {
    // Previous style: cyan frame with blue blocks at the corners.
    for (uint8_t x=2;x<=13;x++) { putPixel(x,1,cyan); putPixel(x,14,cyan); }
    for (uint8_t y=2;y<=13;y++) { putPixel(1,y,cyan); putPixel(14,y,cyan); }
    for (uint8_t y=0;y<3;y++) for (uint8_t x=0;x<3;x++) {
      putPixel(x,y,blue); putPixel(15-x,y,blue); putPixel(x,15-y,blue); putPixel(15-x,15-y,blue);
    }
  } else {
    // TWO continuous frames, each exactly one pixel wide.
    // Pure-blue outer frame and pure-cyan inner frame.
    const CRGB outerBlue(0,0,255);
    const CRGB innerCyan(0,255,255);

    for (uint8_t x=0;x<16;x++) {
      putPixel(x,0,outerBlue);
      putPixel(x,15,outerBlue);
    }
    for (uint8_t y=0;y<16;y++) {
      putPixel(0,y,outerBlue);
      putPixel(15,y,outerBlue);
    }

    for (uint8_t x=1;x<15;x++) {
      putPixel(x,1,innerCyan);
      putPixel(x,14,innerCyan);
    }
    for (uint8_t y=1;y<15;y++) {
      putPixel(1,y,innerCyan);
      putPixel(14,y,innerCyan);
    }
  }
}

void drawQuadrantBorder() {
  CRGB red(255,30,20), yellow(255,255,40), green(70,255,50), blue(30,80,255);
  for (uint8_t x=0;x<8;x++) putPixel(x,0,red);
  for (uint8_t x=8;x<16;x++) putPixel(x,0,yellow);
  for (uint8_t y=0;y<8;y++) putPixel(0,y,red);
  for (uint8_t y=8;y<16;y++) putPixel(0,y,blue);
  for (uint8_t y=0;y<8;y++) putPixel(15,y,yellow);
  for (uint8_t y=8;y<16;y++) putPixel(15,y,green);
  for (uint8_t x=0;x<8;x++) putPixel(x,15,blue);
  for (uint8_t x=8;x<16;x++) putPixel(x,15,green);
}

void drawHourglassIcon() {
  CRGB orange(255,155,0), sand(255,220,80), white(255,255,255);
  // 5x7 icon on the left, matching the observed position.
  for (uint8_t x=0;x<=4;x++) { putPixel(x,8,orange); putPixel(x,14,orange); }
  putPixel(0,9,orange); putPixel(4,9,orange);
  putPixel(1,10,orange); putPixel(3,10,orange);
  putPixel(2,11,white);
  putPixel(1,12,white); putPixel(2,12,white); putPixel(3,12,white);
  putPixel(0,13,sand); putPixel(1,13,sand); putPixel(2,13,sand); putPixel(3,13,sand); putPixel(4,13,sand);
}

void renderClockValues(uint8_t h, uint8_t m) {
  clearFramebuffer();

#if DEBUG_SERIAL
#endif

  switch (clockStyle & 0x07) {
    case 0: { // rainbow frame, selected digits
      drawRainbowBorder();
      drawTimeTwoRows(h,m,clockColor,clockColor,true,2,-1);
      break;
    }
    case 1: { // Christmas
      CRGB red(255,0,0);
      drawDigit3x5(h/10,2,1,red); drawDigit3x5(h%10,6,1,red); drawColon(clockRenderingDate ? 10 : 11,1,red);
      drawChristmasTree();
      drawDigit3x5(m/10,7,9,red); drawDigit3x5(m%10,11,9,red);
      break;
    }
    case 2: { // racing/checker
      drawCheckerRows();
      CRGB orange(255,170,0);
      drawDigit3x5(h/10,0,5,orange); drawDigit3x5(h%10,4,5,orange);
      if (clockRenderingDate) {
        putPixel(9,5,CRGB::White); putPixel(9,6,CRGB::White);
        putPixel(8,7,CRGB::White); putPixel(8,8,CRGB::White);
      } else {
        if (clockColonVisible) { putPixel(8,6,CRGB::White); putPixel(8,8,CRGB::White); }
      }
      drawDigit3x5(m/10,9,5,orange); drawDigit3x5(m%10,13,5,orange);
      break;
    }
    case 3: { // inverted blue/selected background
      clearFramebuffer(clockColor);
      CRGB black(0,0,0);
      drawTimeTwoRows(h,m,black,black,true,2,-1);
      break;
    }
    case 4: { // hourglass
      drawDigit3x5(h/10,2,1,clockColor); drawDigit3x5(h%10,6,1,clockColor); drawColon(clockRenderingDate ? 11 : 10,1,clockColor);
      drawHourglassIcon();
      drawDigit3x5(m/10,6,9,clockColor); drawDigit3x5(m%10,10,9,clockColor);
      break;
    }
    case 5: { // cyan rounded-ish frame + blue corners + orange digits
      drawFrameStyle5(true);
      CRGB orange(255,165,0);
      drawTimeTwoRows(h,m,orange,orange,true,2,0);
      break;
    }
    case 6: { // double frame: blue outer + cyan inner
      // Draw digits first and frames afterward so both borders remain
      // complete and cannot be overwritten.
      drawTimeTwoRows(h,m,clockColor,clockColor,true,2,-1);
      drawFrameStyle5(false);
      break;
    }
    case 7: { // RGBY quadrant frame + selected digits
      drawQuadrantBorder();
      drawTimeTwoRows(h,m,clockColor,clockColor,true,2,-1);
      break;
    }
  }

  scaleLegacy16CanvasToLogical();
  refreshMatrix();
}


void renderDateDDMM() {
  uint16_t y; uint8_t mo,d,h,mi,se;
  getAlarmDateTime(y,mo,d,h,mi,se);
  // Reuse the *unchanged* selected clock renderer/effect. The only
  // difference is that drawColon() renders '/' while this flag is true.
  clockRenderingDate = true;
  renderClockValues(d, mo);
  clockRenderingDate = false;
}

void renderClock() {
  // If the app enables date display, show 30 s of time followed by 5 s DD/MM.
  if (clockShowDate) {
    const uint32_t phase = (millis() - clockCycleStartedAt) % 35000UL;
    if (phase >= 30000UL) { renderDateDDMM(); return; }
  }
  uint8_t h,m,s;
  getCurrentTime(h,m,s);
  clockColonVisible = ((s & 0x01) == 0);
  if (!clock24h) { if (h==0) h=12; else if (h>12) h-=12; }
  renderClockValues(h,m);
}

// ======================================================
// MM:SS / SCOREBOARD
// ======================================================
void renderMMSS(uint32_t sec, const CRGB &c) {
  uint8_t mm=(sec/60)%100, ss=sec%60;
  clearFramebuffer();
  drawDigit3x5(mm/10,0,5,c); drawDigit3x5(mm%10,3,5,c); drawColon(7,5,c);
  drawDigit3x5(ss/10,9,5,c); drawDigit3x5(ss%10,12,5,c);
  scaleLegacy16CanvasToLogical();
  refreshMatrix();
}

void drawCountdownTimerIcon(uint8_t phase) {
  const CRGB rim(255,145,0);
  const CRGB hand(255,45,20);
  const CRGB center(255,220,120);
  const int8_t cx=7, cy=4;

  // 9x9 pixel-art timer in the upper half of the 16x16 canvas.
  putPixel(6,0,rim); putPixel(7,0,rim); putPixel(8,0,rim);
  putPixel(7,1,rim);
  putPixel(4,1,rim); putPixel(10,1,rim);
  putPixel(3,2,rim); putPixel(11,2,rim);
  putPixel(2,3,rim); putPixel(12,3,rim);
  putPixel(2,4,rim); putPixel(12,4,rim);
  putPixel(2,5,rim); putPixel(12,5,rim);
  putPixel(3,6,rim); putPixel(11,6,rim);
  putPixel(4,7,rim); putPixel(10,7,rim);
  putPixel(5,8,rim); putPixel(6,8,rim); putPixel(7,8,rim); putPixel(8,8,rim); putPixel(9,8,rim);

  static const int8_t hx[8]={ 7,10,11,10,7,4,3,4 };
  static const int8_t hy[8]={ 1, 2, 4, 6,7,6,4,2 };
  int8_t ex=hx[phase&7], ey=hy[phase&7];
  // One intermediate pixel keeps diagonal hands visually connected.
  putPixel(cx,cy,center);
  putPixel((cx+ex)/2,(cy+ey)/2,hand);
  putPixel(ex,ey,hand);
}

void renderCountdown(uint32_t remainMs) {
  const uint32_t remainSec=(remainMs+999UL)/1000UL;
  const CRGB digits = remainSec<=5 ? CRGB::Red : CRGB::White;
  const uint8_t mm=(remainSec/60UL)%100UL, ss=remainSec%60UL;
  clearFramebuffer();

  // Countdown decreases, so invert the phase to keep the hand rotating clockwise.
  const uint8_t phase=(uint8_t)((8U-((remainMs/125UL)&7U))&7U);
  drawCountdownTimerIcon(phase);

  // Full-width MM:SS below the timer, rows 10..14.
  drawDigit3x5(mm/10,0,10,digits); drawDigit3x5(mm%10,3,10,digits);
  drawColon(7,10,digits);
  drawDigit3x5(ss/10,9,10,digits); drawDigit3x5(ss%10,12,10,digits);
  scaleLegacy16CanvasToLogical();
  refreshMatrix();
}

void renderStopwatch(uint32_t elapsedMs) {
  const uint32_t elapsedSec=elapsedMs/1000UL;
  const uint8_t mm=(elapsedSec/60UL)%100UL, ss=elapsedSec%60UL;
  clearFramebuffer();

  // Same timer face as countdown, but the hand advances with elapsed time.
  const uint8_t phase=(uint8_t)((elapsedMs/125UL)&7U);
  drawCountdownTimerIcon(phase);

  const CRGB digits=CRGB::White;
  drawDigit3x5(mm/10,0,10,digits); drawDigit3x5(mm%10,3,10,digits);
  drawColon(7,10,digits);
  drawDigit3x5(ss/10,9,10,digits); drawDigit3x5(ss%10,12,10,digits);
  scaleLegacy16CanvasToLogical();
  refreshMatrix();
}

void drawScore2Digit(uint16_t score, int16_t x, int16_t y, const CRGB &c) {
  score%=100;
  if (score>=10) drawDigit3x5(score/10,x,y,c);
  drawDigit3x5(score%10,x+3,y,c);
}

void renderScoreboard() {
  clearFramebuffer();
  drawScore2Digit(scoreA,0,5,CRGB::Blue);
  drawColon(7,5,CRGB::White);
  drawScore2Digit(scoreB,9,5,CRGB::Red);
  scaleLegacy16CanvasToLogical();
  refreshMatrix();
}

// ======================================================
// TEXT
// ======================================================
CRGB getTextPixelColor(int16_t x, int16_t y) {
  uint32_t t=millis();
  if (textState.colorMode<=1) return CRGB(textState.colorR,textState.colorG,textState.colorB);
  if (textState.colorMode==2) return CHSV(x*18+t/18,255,255);
  if (textState.colorMode==3) return CHSV(y*19-t/22,175,255);
  if (textState.colorMode==4) return CHSV(map(sin8(x*20+y*12+t/8),0,255,0,42),220,255);
  return CHSV(map(sin8(x*16-y*10+t/10),0,255,125,205),220,255);
}

bool isTextPixel(uint8_t glyph,uint8_t row,uint8_t col) {
  if (glyph>=textState.glyphCount || row>=textState.glyphHeight || col>=textState.glyphWidth) return false;
  const uint8_t bytesPerRow=(textState.glyphWidth+7)/8;
  const uint16_t off=(uint16_t)row*bytesPerRow+(col>>3);
  if(off>=textState.glyphBytes) return false;
  return textState.bitmap[glyph][off] & (1U << (col & 7)); // LSB = leftmost pixel in each byte
}

uint8_t textVisibleGlyphCapacity() {
  if (textState.glyphAdvance == 0) return 1;
  uint16_t capacity = MATRIX_WIDTH / textState.glyphAdvance;
  if (capacity == 0) capacity = 1;
  if (capacity > textState.glyphCount) capacity = textState.glyphCount;
  return (uint8_t)capacity;
}

bool textUsesPagedViewport() {
  // LEFT/RIGHT (1/2) already traverse the complete line continuously.
  // Every other text effect uses a viewport when the full glyph stream does
  // not fit the matrix.
  return textState.motionEffect != 1 && textState.motionEffect != 2;
}

void advanceTextPage() {
  const uint8_t capacity = textVisibleGlyphCapacity();
  if (capacity == 0 || textState.glyphCount <= capacity) {
    textFirstVisibleGlyph = 0;
    return;
  }
  uint16_t next = (uint16_t)textFirstVisibleGlyph + capacity;
  textFirstVisibleGlyph = (next >= textState.glyphCount) ? 0 : (uint8_t)next;
  textPageChangedAt = millis();
}

uint8_t nextTextPageFirstGlyph(uint8_t firstGlyph) {
  const uint8_t capacity = textVisibleGlyphCapacity();
  if (capacity == 0 || textState.glyphCount <= capacity) return 0;
  uint16_t next = (uint16_t)firstGlyph + capacity;
  return (next >= textState.glyphCount) ? 0 : (uint8_t)next;
}

void drawTextPage(uint8_t firstGlyph, int16_t pageX, int16_t pageY,
                  uint8_t scale=255, int16_t laserRow=-1) {
  const uint8_t capacity = textVisibleGlyphCapacity();
  const uint8_t glyphsToDraw =
      min(capacity, (uint8_t)(textState.glyphCount - firstGlyph));

  for (uint8_t local=0; local<glyphsToDraw; local++) {
    const uint8_t g = firstGlyph + local;
    int16_t gx=pageX+(int16_t)local*textState.glyphAdvance;
    int16_t gy=pageY;
    for (uint8_t row=0;row<textState.glyphHeight;row++) for (uint8_t col=0;col<textState.glyphWidth;col++) {
      if (!isTextPixel(g,row,col)) continue;
      int16_t px=gx+col, py=gy+row;
      CRGB c=getTextPixelColor(px,py);
      if (scale<255) c.nscale8_video(scale);
      if (laserRow>=0 && py==laserRow) c=CRGB::White;
      putPixel(px,py,c);
    }
  }
}

void drawTextGlyphs(uint8_t scale=255,int16_t laserRow=-1) {
  if (!textUsesPagedViewport()) {
    // LEFT/RIGHT retain their historical full-line continuous renderer.
    for (uint8_t g=0; g<textState.glyphCount; g++) {
      int16_t gx=textState.offsetX+(int16_t)g*textState.glyphAdvance;
      int16_t gy=textState.offsetY;
      for (uint8_t row=0;row<textState.glyphHeight;row++) for (uint8_t col=0;col<textState.glyphWidth;col++) {
        if (!isTextPixel(g,row,col)) continue;
        int16_t px=gx+col, py=gy+row;
        CRGB c=getTextPixelColor(px,py);
        if (scale<255) c.nscale8_video(scale);
        if (laserRow>=0 && py==laserRow) c=CRGB::White;
        putPixel(px,py,c);
      }
    }
    return;
  }

  drawTextPage(textFirstVisibleGlyph, textState.offsetX, textState.offsetY, scale, laserRow);

  // UP/DOWN are a continuous vertical tape. Draw the next page immediately
  // behind the current one with a one-pixel separator so the matrix never
  // enters a fully blank interval between pages.
  if ((textState.motionEffect == 3 || textState.motionEffect == 4) &&
      textState.glyphCount > textVisibleGlyphCapacity()) {
    const uint8_t nextFirst = nextTextPageFirstGlyph(textFirstVisibleGlyph);
    const int16_t step = (int16_t)textState.glyphHeight + 1;
    const int16_t nextY = (textState.motionEffect == 3)
                        ? textState.offsetY + step
                        : textState.offsetY - step;
    drawTextPage(nextFirst, textState.offsetX, nextY, scale, laserRow);
  }
}

void renderTextFrame() {
  if (!textState.valid) return;
  CRGB bg = textState.backgroundMode ? CRGB(textState.backgroundR,textState.backgroundG,textState.backgroundB) : CRGB::Black;
  clearFramebuffer(bg);
  uint32_t e=millis()-textState.animationStart;
  switch (textState.motionEffect) {
    default: case 0: case 1: case 2: case 3: case 4: drawTextGlyphs(); break;
    case 5: if (((e/350)&1)==0) drawTextGlyphs(); break;
    case 6: drawTextGlyphs(40+scale8(sin8(e/8),215)); break;
    case 7: {
      drawTextGlyphs();
      const uint16_t ph=e/100U;
      // A regular index*3 Y phase creates visible travelling snow bands,
      // especially on a 16x16 logical canvas upscaled to a larger panel.
      // Use deterministic irregular phases and two fall rates instead, with
      // particle density proportional to the logical display width.
      const uint8_t flakeCount=MATRIX_WIDTH>=64 ? 48U : (MATRIX_WIDTH>=32 ? 24U : 16U);
      for(uint8_t i=0;i<flakeCount;i++){
        const uint32_t seed=effectHash((uint32_t)i*0x9E3779B9UL+0x51ED270BUL);
        const uint8_t x=(uint8_t)(seed%MATRIX_WIDTH);
        const uint16_t startY=(uint16_t)((seed>>8)%MATRIX_HEIGHT);
        const uint8_t rate=(uint8_t)(1U+((seed>>20)&0x01U));
        const uint8_t y=(uint8_t)((startY+(uint32_t)ph*rate)%MATRIX_HEIGHT);
        putPixel(x,y,CRGB::White);
      }
      break;
    }
    case 8: {
      int16_t lr=(e/70)%MATRIX_HEIGHT;
      drawTextGlyphs(110,lr);
      for(uint8_t x=0;x<MATRIX_WIDTH;x++){ CRGB c=CRGB::Red; c.nscale8_video(120); putPixel(x,lr,c); }
      break;
    }
  }
  refreshMatrix();
}

uint16_t textFrameInterval() {
  uint8_t s=min((uint8_t)100,textState.speed);
  return map(s,0,100,140,20);
}

uint16_t textPageInterval() {
  // PIN uses the same app speed setting as the motion effects, but as a
  // readable page dwell rather than as a per-pixel frame interval.
  uint8_t s=min((uint8_t)100,textState.speed);
  return map(s,0,100,1600,250);
}

void resetTextPosition() {
  int tw=textState.glyphCount*textState.glyphAdvance;
  const int16_t centeredY =
      (MATRIX_HEIGHT > textState.glyphHeight)
          ? (int16_t)((MATRIX_HEIGHT - textState.glyphHeight) / 2)
          : (int16_t)0;
  textFirstVisibleGlyph=0;
  textPageChangedAt=millis();
  switch(textState.motionEffect){
    case 1:textState.offsetX=MATRIX_WIDTH;textState.offsetY=centeredY;break;
    case 2:textState.offsetX=-tw;textState.offsetY=centeredY;break;
    case 3:textState.offsetX=0;textState.offsetY=MATRIX_HEIGHT;break;
    case 4:textState.offsetX=0;textState.offsetY=-textState.glyphHeight;break;
    default:textState.offsetX=0;textState.offsetY=centeredY;break;
  }
  textState.animationStart=millis();
  textState.lastFrame=0;
  textState.lastRenderFrame=0;
}

void updateTextAnimation() {
  if(!textState.valid) return;
  uint32_t now=millis();
  const uint8_t capacity = textVisibleGlyphCapacity();
  const bool hasMorePages = textState.glyphCount > capacity;

  // Static/page-based effects change only the visible glyph window. Do not
  // reset animationStart here: Blink/Breathe/Snow/Laser must keep a continuous
  // phase while the displayed word fragment changes.
  if (textState.motionEffect == 0) {
    if (hasMorePages && (uint32_t)(now-textPageChangedAt) >= textPageInterval()) {
      advanceTextPage();
      renderTextFrame();
      textState.lastRenderFrame=now;
    } else if (textState.colorMode>=2 &&
               (uint32_t)(now-textState.lastRenderFrame) >= 45U) {
      // Dynamic color refresh must not advance text motion.
      renderTextFrame();
      textState.lastRenderFrame=now;
    }
    return;
  }

  if (textState.motionEffect >= 5 && hasMorePages &&
      (uint32_t)(now-textPageChangedAt) >= textPageInterval()) {
    advanceTextPage();
  }

  const uint16_t motionInterval=textFrameInterval();
  bool motionAdvanced=false;

  if ((uint32_t)(now-textState.lastFrame) >= motionInterval) {
    textState.lastFrame=now;
    int tw=textState.glyphCount*textState.glyphAdvance;
    switch(textState.motionEffect){
      case 1:
        if(--textState.offsetX < -tw) textState.offsetX=MATRIX_WIDTH;
        motionAdvanced=true;
        break;
      case 2:
        if(++textState.offsetX > MATRIX_WIDTH) textState.offsetX=-tw;
        motionAdvanced=true;
        break;
      case 3: {
        --textState.offsetY;
        const int16_t step=(int16_t)textState.glyphHeight+1;
        if(textState.offsetY <= -step) {
          textState.offsetY += step;
          advanceTextPage();
        }
        motionAdvanced=true;
        break;
      }
      case 4: {
        ++textState.offsetY;
        const int16_t step=(int16_t)textState.glyphHeight+1;
        if(textState.offsetY >= step) {
          textState.offsetY -= step;
          advanceTextPage();
        }
        motionAdvanced=true;
        break;
      }
    }
  }

  // Dynamic text visuals may need a faster redraw rate, but redraws alone
  // never change X/Y offsets or page position.
  const bool fastVisualRefresh=(textState.motionEffect>=5 || textState.colorMode>=2);
  const bool visualDue=fastVisualRefresh &&
                       (uint32_t)(now-textState.lastRenderFrame) >= 45U;

  if (motionAdvanced || visualDue) {
    renderTextFrame();
    textState.lastRenderFrame=now;
  }
}

// TEXT glyph records observed from the official app:
// marker 0x02/0x03:  8x16 bitmap,  4-byte meta +  16 bitmap bytes =  20 bytes/glyph
// marker 0x05/0x06: 16x32 bitmap,  4-byte meta +  64 bitmap bytes =  68 bytes/glyph
// marker 0x08/0x09: 32x64 bitmap,  4-byte meta + 256 bitmap bytes = 260 bytes/glyph
// SimSun/SimHei are rasterized by the app: no device-side font selection is required.
// The exact-record-size fallback mirrors the hardware-validated WLED Usermod path
// and tolerates equivalent app/firmware marker variants (for example 0x0A).
bool parseTextPayloadInternal(const uint8_t *data,size_t len,bool carouselContext) {
  if(len<TEXT_GLOBAL_HEADER) return false;
  uint8_t requested=data[0];
  uint8_t n=min(requested,(uint8_t)MAX_TEXT_GLYPHS);
  if(!n) return false;

  const uint8_t marker=data[TEXT_GLOBAL_HEADER];
  uint8_t glyphWidth=0,glyphHeight=0;
  uint16_t glyphBytes=0;
  if(marker==0x02 || marker==0x03){ glyphWidth=8; glyphHeight=16; glyphBytes=16; }
  else if(marker==0x05 || marker==0x06){ glyphWidth=16; glyphHeight=32; glyphBytes=64; }
  else if(marker==0x08 || marker==0x09){ glyphWidth=32; glyphHeight=64; glyphBytes=256; }
  else {
    const size_t payloadBytes=len-TEXT_GLOBAL_HEADER;
    if(payloadBytes % requested != 0) {
#if DEBUG_SERIAL
      Serial.print("TEXT unsupported glyph marker 0x"); Serial.println(marker,HEX);
#endif
      return false;
    }
    const size_t inferredRecordBytes=payloadBytes/requested;
    if(inferredRecordBytes<TEXT_GLYPH_META) return false;
    const size_t inferredGlyphBytes=inferredRecordBytes-TEXT_GLYPH_META;
    if(inferredGlyphBytes==16){ glyphWidth=8; glyphHeight=16; glyphBytes=16; }
    else if(inferredGlyphBytes==64){ glyphWidth=16; glyphHeight=32; glyphBytes=64; }
    else if(inferredGlyphBytes==256){ glyphWidth=32; glyphHeight=64; glyphBytes=256; }
    else {
#if DEBUG_SERIAL
      Serial.print("TEXT unsupported glyph marker 0x"); Serial.print(marker,HEX);
      Serial.print(" recordBytes="); Serial.println(inferredRecordBytes);
#endif
      return false;
    }
  }
  const size_t recordBytes=TEXT_GLYPH_META+(size_t)glyphBytes;
  const size_t need=TEXT_GLOBAL_HEADER+(size_t)requested*recordBytes;
  if(len<need) return false;

  textState.valid=true; textState.glyphCount=n;
  textState.glyphWidth=glyphWidth; textState.glyphHeight=glyphHeight;
  textState.glyphBytes=glyphBytes; textState.glyphAdvance=glyphWidth;
  textState.motionEffect=data[4]; textState.speed=data[5]; textState.colorMode=data[6];
  textState.colorR=data[7]; textState.colorG=data[8]; textState.colorB=data[9];
  textState.backgroundMode=data[10]; textState.backgroundR=data[11]; textState.backgroundG=data[12]; textState.backgroundB=data[13];
  for(uint8_t g=0;g<n;g++){
    size_t base=TEXT_GLOBAL_HEADER+(size_t)g*recordBytes;
    for(uint8_t i=0;i<TEXT_GLYPH_META;i++) textState.meta[g][i]=data[base+i];
    memset(textState.bitmap[g],0,TEXT_MAX_BITMAP_BYTES);
    memcpy(textState.bitmap[g],data+base+TEXT_GLYPH_META,glyphBytes);
  }
#if DEBUG_SERIAL
  Serial.print("TEXT glyphs=");Serial.print(n);Serial.print(" size=");Serial.print(glyphWidth);Serial.print('x');Serial.print(glyphHeight);
  Serial.print(" marker=0x");Serial.println(marker,HEX);
#endif
  resetTextPosition();
  if(carouselContext){
    // Carousel TEXT is still a carousel slot. Do not call switchDisplayMode(),
    // because that intentionally tears down carousel/upload state for ordinary
    // live TEXT commands. The caller has already stopped any GIF decoder.
    displayMode=DISPLAY_TEXT;
  } else {
    switchDisplayMode(DISPLAY_TEXT);
  }
  renderTextFrame();
  return true;
}

void parseTextPayload(const uint8_t *data,size_t len) {
  (void)parseTextPayloadInternal(data,len,false);
}

// ======================================================
// LIGHT EFFECTS
// ======================================================
// Band-based effects were originally tuned on the 16x16 profile. Preserve
// their apparent geometry on larger logical displays by scaling band widths
// with the logical iDotMatrix resolution: 16x16=1x, 32x32=2x, 64x64=4x.
static inline uint8_t effectBandScale(){
  return (MATRIX_WIDTH >= 64) ? 4 : ((MATRIX_WIDTH >= 32) ? 2 : 1);
}

uint8_t effectChannelTo8(uint8_t v){ return v>=127 ? 255 : v*2; }
CRGB getEffectColor(uint8_t i){ if(!effectState.colorCount) return CRGB::White; return effectState.colors[i%effectState.colorCount]; }

CRGB effectPaletteGradient(uint8_t p){
  if(!effectState.colorCount) return CRGB::Black;
  if(effectState.colorCount==1) return effectState.colors[0];
  uint16_t scaled=(uint16_t)p*effectState.colorCount;
  uint8_t idx=scaled>>8, frac=scaled&0xFF;
  return blend(effectState.colors[idx%effectState.colorCount],effectState.colors[(idx+1)%effectState.colorCount],frac);
}

// Slower speed mapping than the previous build.
uint16_t effectFrameInterval(){
  uint8_t s=min((uint8_t)100,effectState.speed);

  // Effect 6 is a true per-pixel cross-fade, so smooth motion requires
  // frequent refreshes even when the app-selected speed is very low
  // (for example speed=5).
  if(effectState.effect==6) return 40;

  // Gli altri effetti restano volutamente piu' lenti.
  return map(s,0,100,360,70);
}

uint32_t effectPhase(){
  uint16_t multiplier=4+(effectState.speed/3);
  return ((millis()-effectState.startMillis)*multiplier)/100UL;
}

uint32_t effectHash(uint32_t v){ v^=v>>16; v*=0x7FEB352DUL; v^=v>>15; v*=0x846CA68BUL; v^=v>>16; return v; }

void renderEffect0(){
  uint32_t ph=effectPhase()/4;
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++) {
    // Long gradient: roughly 10+ pixels per color transition.
    uint8_t pos=(uint8_t)((y*5 + x + ph)/3);
    framebuffer[logicalIndex(x,y)]=effectPaletteGradient(pos);
  }
}

void renderEffect1(){
  clearFramebuffer(); uint32_t frame=effectPhase()/8;
  for(uint8_t i=0;i<22;i++){
    uint32_t h=effectHash((uint32_t)i*173UL+frame*31UL);
    uint8_t x=(uint8_t)(h%MATRIX_WIDTH),y=(uint8_t)((h>>8)%MATRIX_HEIGHT);
    CRGB c=getEffectColor((h>>8)%max((uint8_t)1,effectState.colorCount));
    c.nscale8_video(100+((h>>16)&0x9F)); putPixel(x,y,c);
  }
}

void renderEffect2(){
  uint32_t ph=effectPhase()/3;
  // Background: continuous fading between colors.
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++){
    uint8_t pos=(uint8_t)(ph+x*2+y*2);
    CRGB c=effectPaletteGradient(pos); c.nscale8_video(190);
    framebuffer[logicalIndex(x,y)]=c;
  }
  // Spikes are always white.
  uint32_t frame=ph/4;
  for(uint8_t i=0;i<18;i++){
    uint32_t h=effectHash((uint32_t)i*223UL+frame*19UL);
    putPixel((uint8_t)(h%MATRIX_WIDTH),(uint8_t)((h>>8)%MATRIX_HEIGHT),CRGB::White);
  }
}

void renderEffect3(){
  // The observed style advances the bands in two-pixel steps.
  uint32_t raw=effectPhase()/10;
  uint32_t ph=(raw/2)*2;
  uint8_t count=max((uint8_t)1,effectState.colorCount);
  const uint8_t stripeWidth=(uint8_t)(4U*effectBandScale());
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++)
    framebuffer[logicalIndex(x,y)]=getEffectColor(((x+ph)/stripeWidth)%count);
}

void renderEffect4(){
  // Diagonals also advance in two-pixel steps.
  uint32_t raw=effectPhase()/10;
  uint32_t ph=(raw/2)*2;
  uint8_t count=max((uint8_t)1,effectState.colorCount);
  const uint8_t stripeWidth=(uint8_t)(4U*effectBandScale());
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++)
    framebuffer[logicalIndex(x,y)]=getEffectColor(((x+y+ph)/stripeWidth)%count);
}

void renderEffect5(){
  clearFramebuffer();
  uint32_t raw=effectPhase()/11;
  uint32_t ph=(raw/2)*2;
  uint8_t count=max((uint8_t)1,effectState.colorCount);
  const uint8_t scale=effectBandScale();
  const uint8_t colorWidth=(uint8_t)(5U*scale);
  const uint8_t blackWidth=(uint8_t)(4U*scale);
  const uint8_t block=(uint8_t)(colorWidth+blackWidth);
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++){
    uint16_t d=x+y+ph; uint8_t within=d%block;
    if(within<colorWidth) putPixel(x,y,getEffectColor((d/block)%count));
  }
}

void renderEffect6(){
  // UI effect 7: each pixel keeps its own phase and genuinely fades
  // from one color to the next instead of switching instantly.
  const uint8_t count=max((uint8_t)1,effectState.colorCount);

  if(count==1){
    clearFramebuffer(getEffectColor(0));
    return;
  }

  // In the app speed=5 is slow: one transition lasts about 5.7 seconds,
  // while speed=100 drops to about 700 ms. A 40 ms update interval gives
  // the fade enough steps to remain visibly continuous at minimum speed.
  const uint8_t sp=constrain(effectState.speed,0,100);
  const uint32_t fadeMs=map(sp,0,100,6000,700);
  const uint32_t elapsed=millis()-effectState.startMillis;
  const uint32_t cycleMs=fadeMs*(uint32_t)count;

  for(uint8_t y=0;y<MATRIX_HEIGHT;y++){
    for(uint8_t x=0;x<MATRIX_WIDTH;x++){
      const uint16_t i=(uint16_t)y*MATRIX_WIDTH+x;
      const uint32_t seed=effectHash((uint32_t)i*977UL+0x51EDUL);

      // Each pixel starts at a different point in the same continuous
      // palette cycle.
      const uint32_t local=(elapsed+(seed%cycleMs))%cycleMs;
      const uint8_t a=(uint8_t)(local/fadeMs);
      const uint8_t b=(uint8_t)((a+1)%count);
      const uint32_t within=local%fadeMs;

      // 0..255 with high temporal resolution.
      const uint8_t linear=(uint8_t)((within*255UL)/(fadeMs-1UL));
      const uint8_t frac=ease8InOutCubic(linear);

      framebuffer[logicalIndex(x,y)] = blend(
        getEffectColor(a),
        getEffectColor(b),
        frac
      );
    }
  }
}

void renderEffect(){
  if(!effectState.valid) return;
  switch(effectState.effect){
    case 0:renderEffect0();break; case 1:renderEffect1();break; case 2:renderEffect2();break;
    case 3:renderEffect3();break; case 4:renderEffect4();break; case 5:renderEffect5();break; case 6:renderEffect6();break;
    default:clearFramebuffer();break;
  }
  refreshMatrix();
}

void updateEffect(){
  if(displayMode!=DISPLAY_EFFECT || !effectState.valid) return;
  uint32_t now=millis();
  if(now-effectState.lastFrameMillis<effectFrameInterval()) return;
  effectState.lastFrameMillis=now; renderEffect();
}

bool processEffectCommand(const uint8_t *data,size_t len){
  if(len<7 || data[2]!=0x03 || data[3]!=0x02) return false;
  uint8_t effect=data[4], speed=data[5], count=data[6];
  size_t expected=7+(size_t)count*3;
  if(len<expected){ sendCommandAck(0x03,0x02); return true; }
  count=min((uint8_t)MAX_EFFECT_COLORS,count);
  effectState.valid=true; effectState.effect=effect; effectState.speed=speed; effectState.colorCount=count;
  for(uint8_t i=0;i<count;i++){
    size_t p=7+(size_t)i*3;
    effectState.colors[i]=CRGB(effectChannelTo8(data[p]),effectChannelTo8(data[p+1]),effectChannelTo8(data[p+2]));
  }
  effectState.startMillis=millis(); effectState.lastFrameMillis=0;
#if DEBUG_SERIAL
  Serial.print("EFFECT id=");Serial.print(effect);Serial.print(" speed=");Serial.print(speed);Serial.print(" colors=");Serial.println(count);
#endif
  switchDisplayMode(DISPLAY_EFFECT); renderEffect(); sendCommandAck(0x03,0x02); return true;
}

// ======================================================
// GIF
// ======================================================
void destroyGIFDecoder(){
  gifPlaying=false; gifLoaded=false; gifRestartPending=false; gifFrameWasDrawn=false;
  if(gif){
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
    Serial.println("GIF OLD close");
#endif
    gif->close();
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
    Serial.println("GIF OLD delete");
#endif
    delete gif;
    gif=nullptr;
  }
}
void stopGIFPlayback(){
  bool removeEventFile=gifEventPlaybackFileActive;
  destroyGIFDecoder();
  gifEventPlaybackFileActive=false;
  gifCarouselPlaybackFileActive=false;
  gifPresetPlaybackFileActive=false;
  if(removeEventFile && littleFsReady) LittleFS.remove(EVENT_GIF_PLAY_FILE);
}
void freeGIF(){
  pendingGifStart=false;
  pendingGifSlot=-1;
  pendingGifSize=0;
  pendingGifStartAt=0;
  gifDecoderTeardownPending=false;
  gifDecoderOpenPending=false;
  gifDecoderOpenAt=0;
  stopGIFPlayback();
  gifSize=0;
  gifStoredOnFS=false;
}

// AnimatedGIF file callbacks. BUILD 89 keeps all compressed GIF playback
// file-backed, so no GIF requires one large contiguous DRAM allocation.
void *GIFOpenFile(const char *fname, int32_t *pSize){
  if(!littleFsReady) return nullptr;
  File *f=new File(LittleFS.open(fname,"r"));
  if(!f || !(*f)){ if(f) delete f; return nullptr; }
  *pSize=(int32_t)f->size();
  return (void *)f;
}
void GIFCloseFile(void *pHandle){
  File *f=(File *)pHandle;
  if(f){ f->close(); delete f; }
}
int32_t GIFReadFile(GIFFILE *pFile,uint8_t *pBuf,int32_t iLen){
  File *f=(File *)pFile->fHandle;
  if(!f || !(*f) || iLen<=0) return 0;
  int32_t remain=pFile->iSize-pFile->iPos;
  if(remain<=0) return 0;
  if(iLen>remain) iLen=remain;
  int32_t n=(int32_t)f->read(pBuf,(size_t)iLen);
  pFile->iPos=(int32_t)f->position();
  return n;
}
int32_t GIFSeekFile(GIFFILE *pFile,int32_t iPosition){
  File *f=(File *)pFile->fHandle;
  if(!f || !(*f)) return -1;
  if(iPosition<0) iPosition=0;
  if(iPosition>pFile->iSize) iPosition=pFile->iSize;
  if(!f->seek((uint32_t)iPosition,SeekSet)) return -1;
  pFile->iPos=(int32_t)f->position();
  return pFile->iPos;
}

void GIFDraw(GIFDRAW *pDraw){
  if(!pDraw) return; gifFrameWasDrawn=true;
  int y=pDraw->iY+pDraw->y; if(y<0||y>=MATRIX_HEIGHT) return;
  int width=pDraw->iWidth; if(pDraw->iX+width>MATRIX_WIDTH) width=MATRIX_WIDTH-pDraw->iX; if(width<=0) return;
  uint8_t *pixels=pDraw->pPixels; uint16_t *palette=pDraw->pPalette; if(!palette) return;
  if(pDraw->ucDisposalMethod==2){
    for(int x=0;x<width;x++) if(pixels[x]==pDraw->ucTransparent) pixels[x]=pDraw->ucBackground;
    pDraw->ucHasTransparency=0;
  }
  for(int x=0;x<width;x++){
    int sx=pDraw->iX+x; if(sx<0||sx>=MATRIX_WIDTH) continue;
    uint8_t pi=pixels[x]; if(pDraw->ucHasTransparency && pi==pDraw->ucTransparent) continue;
    uint16_t rgb=palette[pi];
    uint8_t r=((rgb>>11)&0x1F)*255/31, g=((rgb>>5)&0x3F)*255/63, b=(rgb&0x1F)*255/31;
    gifFrame[logicalIndex(sx,y)]=CRGB(r,g,b);
  }
}

bool startGIFFile(const char *path, bool eventPlayback, bool carouselPlayback){
  if(!littleFsReady || !path || !gifSize || !LittleFS.exists(path)) return false;
  if(gif) destroyGIFDecoder();
  fill_solid(gifFrame,NUM_LEDS,CRGB::Black);
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
  Serial.println("GIF NEW alloc");
#endif
  gif=new AnimatedGIF();
  if(!gif){
#if DEBUG_SERIAL
    Serial.println("GIF NEW alloc FAILED");
#endif
    return false;
  }
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
  Serial.println("GIF NEW begin");
#endif
  gif->begin(LITTLE_ENDIAN_PIXELS); gif->setDrawType(GIF_DRAW_RAW);
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
  Serial.println("GIF NEW open");
#endif
  bool opened=gif->open(path,GIFOpenFile,GIFCloseFile,GIFReadFile,GIFSeekFile,GIFDraw);
  if(!opened){
#if DEBUG_SERIAL
    Serial.println("GIF NEW open FAILED");
#endif
    destroyGIFDecoder();
    return false;
  }
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
  Serial.println("GIF NEW open OK");
  Serial.println("GIF NEW reset");
#endif
  gif->reset();
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
  Serial.println("GIF NEW reset OK");
#endif
  gifStoredOnFS=true;
  gifEventPlaybackFileActive=eventPlayback;
  gifCarouselPlaybackFileActive=carouselPlayback;
  gifLoaded=gifPlaying=true; gifNextFrameAt=millis(); displayMode=DISPLAY_GIF; return true;
}

bool startGIF(){
  if(presetActive) stopPresetPlayback();
  carouselActive=false;
  carouselActiveSlot=-1;
  carouselStartPending=false;
  carouselEnterRequested=false;
  carouselUploadOpen=false;
  carouselUploadBlackout=false;
  endCarouselTransferIndicator();
  return startGIFFile(GIF_PLAY_FILE,false,false);
}

bool startStoredGIFPlayback(const String &sourcePath, uint32_t expectedSize, uint32_t expectedCRC){
  if(presetActive) stopPresetPlayback();
  carouselActive=false;
  carouselActiveSlot=-1;
  carouselStartPending=false;
  carouselEnterRequested=false;
  carouselUploadOpen=false;
  carouselUploadBlackout=false;
  endCarouselTransferIndicator();
  if(!littleFsReady || !expectedSize) return false;
  File src=LittleFS.open(sourcePath,"r");
  if(!src || (uint32_t)src.size()!=expectedSize){ if(src) src.close(); return false; }

  // Never decode an Alarm/Schedule source file directly: it can later be
  // renamed as part of a transactional update. Copy it to a disposable PLAY
  // file so AnimatedGIF owns a stable file for the whole playback lifetime.
  stopGIFPlayback();
  LittleFS.remove(EVENT_GIF_PLAY_FILE);
  size_t fsTotal=LittleFS.totalBytes();
  size_t fsUsed=LittleFS.usedBytes();
  size_t fsFree=fsTotal>fsUsed ? fsTotal-fsUsed : 0;
  if((uint64_t)expectedSize>(uint64_t)fsFree){
#if DEBUG_SERIAL
    Serial.print("EVENT GIF rejected: bytes="); Serial.print(expectedSize);
    Serial.print(" LittleFS free="); Serial.println(fsFree);
#endif
    src.close();
    return false;
  }

  File dst=LittleFS.open(EVENT_GIF_PLAY_FILE,"w");
  if(!dst){ src.close(); return false; }
  uint8_t buf[512];
  uint32_t remaining=expectedSize;
  uint32_t runningCRC=0xFFFFFFFF;
  bool ok=true;
  while(remaining && ok){
    size_t want=min((size_t)remaining,sizeof(buf));
    int n=src.read(buf,want);
    if(n<=0){ ok=false; break; }
    runningCRC=crc32Update(runningCRC,buf,(size_t)n);
    if(dst.write(buf,(size_t)n)!=(size_t)n){ ok=false; break; }
    remaining-=(uint32_t)n;
  }
  dst.flush();
  dst.close();
  src.close();
  runningCRC^=0xFFFFFFFF;
  if(!ok || remaining || runningCRC!=expectedCRC || storedFileSize(EVENT_GIF_PLAY_FILE)!=expectedSize){
#if DEBUG_SERIAL
    Serial.println("EVENT GIF copy/CRC failed");
#endif
    LittleFS.remove(EVENT_GIF_PLAY_FILE);
    return false;
  }

  gifSize=expectedSize;
  gifStoredOnFS=true;
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
  Serial.print("EVENT GIF PLAY copy bytes="); Serial.print(expectedSize);
  Serial.print(" source="); Serial.println(sourcePath);
#endif
  if(!startGIFFile(EVENT_GIF_PLAY_FILE,true,false)){
    gifStoredOnFS=false;
    gifSize=0;
    LittleFS.remove(EVENT_GIF_PLAY_FILE);
    return false;
  }
  return true;
}

void updateGIF(){
  if(!gif || !gifPlaying||!gifLoaded||displayMode!=DISPLAY_GIF) return;
  uint32_t now=millis(); if((long)(now-gifNextFrameAt)<0) return;
  if(gifRestartPending){ gif->reset(); gifRestartPending=false; fill_solid(gifFrame,NUM_LEDS,CRGB::Black); }
  gifFrameWasDrawn=false; int delayMs=0; int result=gif->playFrame(false,&delayMs);
  if(!gifFrameWasDrawn){ gif->reset(); gifNextFrameAt=now+10; return; }
  ::memcpy(framebuffer,gifFrame,LOGICAL_FRAME_BYTES); refreshMatrix();
  if(delayMs<10) delayMs=10; gifNextFrameAt=now+delayMs; if(result==0) gifRestartPending=true;
}

// ======================================================
// DEVICE-ASSET CAROUSEL
// ======================================================
String carouselFileName(uint8_t slot, uint8_t dataType) {
  return String("/car") + slot + (dataType==3 ? ".txt" : ".gif");
}
String carouselTempFileName(uint8_t slot) { return String("/car") + slot + ".tmp"; }
String carouselBackupFileName(uint8_t slot) { return String("/car") + slot + ".bak"; }

bool saveCarouselSlotMeta(uint8_t slot) {
  if(!carouselPrefsReady || slot>=CAROUSEL_SLOT_COUNT) return false;
  char key[8]; snprintf(key,sizeof(key),"c%u",slot);
  return carouselPrefs.putBytes(key,&carouselSlots[slot],sizeof(CarouselSlotMeta)) == sizeof(CarouselSlotMeta);
}

void removeCarouselSlotMeta(uint8_t slot) {
  if(slot>=CAROUSEL_SLOT_COUNT) return;
  if(carouselPrefsReady){
    char key[8]; snprintf(key,sizeof(key),"c%u",slot);
    if(carouselPrefs.isKey(key)) carouselPrefs.remove(key);
  }
  carouselSlots[slot]=CarouselSlotMeta();
}

void clearCarouselSlot(uint8_t slot) {
  if(slot>=CAROUSEL_SLOT_COUNT) return;
  if(littleFsReady){
    LittleFS.remove(carouselTempFileName(slot));
    LittleFS.remove(carouselBackupFileName(slot));
    LittleFS.remove(carouselFileName(slot,1));
    LittleFS.remove(carouselFileName(slot,3));
  }
  removeCarouselSlotMeta(slot);
}

void saveCarouselOrder() {
  if(!carouselPrefsReady) return;
  carouselPrefs.putUChar("count",carouselOrderCount);
  carouselPrefs.putBytes("order",carouselOrder,CAROUSEL_SLOT_COUNT);
}

void recoverCarouselSlot(uint8_t slot) {
  if(!littleFsReady || slot>=CAROUSEL_SLOT_COUNT) return;
  String tmp=carouselTempFileName(slot), bak=carouselBackupFileName(slot);
  LittleFS.remove(tmp);
  const CarouselSlotMeta &m=carouselSlots[slot];
  if(!m.configured || !m.mediaSize || (m.dataType!=1 && m.dataType!=3)){
    LittleFS.remove(carouselFileName(slot,1));
    LittleFS.remove(carouselFileName(slot,3));
    LittleFS.remove(bak);
    return;
  }
  String dst=carouselFileName(slot,m.dataType);
  bool dstOK=fileMatchesMedia(dst,m.mediaSize,m.mediaCRC);
  bool bakOK=fileMatchesMedia(bak,m.mediaSize,m.mediaCRC);
  if(dstOK) LittleFS.remove(bak);
  else if(bakOK){
    LittleFS.remove(dst);
    if(!LittleFS.rename(bak,dst)){
#if DEBUG_SERIAL
      Serial.print("CAROUSEL recovery rename failed slot="); Serial.println(slot);
#endif
    }
  }
  // Never leave a stale file of the other supported slot type around.
  LittleFS.remove(carouselFileName(slot,m.dataType==1 ? 3 : 1));
}

void loadCarousel() {
  carouselPrefsReady=carouselPrefs.begin("idot-car",false);
  carouselOrderCount=0;
  for(uint8_t i=0;i<CAROUSEL_SLOT_COUNT;i++) carouselOrder[i]=0;
  if(carouselPrefsReady){
    uint8_t savedCount=carouselPrefs.getUChar("count",0);
    if(savedCount<=CAROUSEL_SLOT_COUNT && carouselPrefs.getBytesLength("order")==CAROUSEL_SLOT_COUNT){
      uint8_t savedOrder[CAROUSEL_SLOT_COUNT];
      carouselPrefs.getBytes("order",savedOrder,CAROUSEL_SLOT_COUNT);
      bool seen[CAROUSEL_SLOT_COUNT]={false};
      bool valid=true;
      for(uint8_t i=0;i<savedCount;i++){
        if(savedOrder[i]>=CAROUSEL_SLOT_COUNT || seen[savedOrder[i]]){ valid=false; break; }
        seen[savedOrder[i]]=true;
      }
      if(valid){
        carouselOrderCount=savedCount;
        memcpy(carouselOrder,savedOrder,CAROUSEL_SLOT_COUNT);
      }
    }
  }
  for(uint8_t i=0;i<CAROUSEL_SLOT_COUNT;i++){
    if(carouselPrefsReady){
      char key[8]; snprintf(key,sizeof(key),"c%u",i);
      if(carouselPrefs.getBytesLength(key)==sizeof(CarouselSlotMeta))
        carouselPrefs.getBytes(key,&carouselSlots[i],sizeof(CarouselSlotMeta));
      else if(carouselPrefs.isKey(key))
        carouselPrefs.remove(key); // discard metadata written by superseded dev layouts
    }
    recoverCarouselSlot(i);
#if DEBUG_SERIAL && CAROUSEL_PROTOCOL_DEBUG
    if(carouselSlots[i].configured){
      Serial.print("CAROUSEL LOAD slot="); Serial.print(i);
      Serial.print(" type="); Serial.print(carouselSlots[i].dataType==3 ? "TEXT" : "GIF");
      Serial.print(" dwell="); Serial.print(carouselSlots[i].dwellSeconds);
      Serial.print(" bytes="); Serial.print(carouselSlots[i].mediaSize);
      String path=carouselFileName(i,carouselSlots[i].dataType);
      Serial.print(" file="); Serial.println(littleFsReady && LittleFS.exists(path) ? "YES" : "NO");
    }
#endif
  }

  // BUILD 92 temporarily allowed slots 12..35. Remove leftovers from that
  // superseded development build so they cannot waste LittleFS/NVS space.
  if(littleFsReady){
    for(uint8_t i=12;i<36;i++){
      LittleFS.remove(String("/car")+i+".gif");
      LittleFS.remove(String("/car")+i+".txt");
      LittleFS.remove(String("/car")+i+".tmp");
      LittleFS.remove(String("/car")+i+".bak");
    }
  }
  if(carouselPrefsReady){
    for(uint8_t i=12;i<36;i++){
      char key[8]; snprintf(key,sizeof(key),"c%u",i);
      if(carouselPrefs.isKey(key)) carouselPrefs.remove(key);
    }
  }
}

void stopCarouselPlayback() {
  carouselStartPending=false;
  carouselActive=false;
  carouselActiveSlot=-1;
  if(gifCarouselPlaybackFileActive){
    stopGIFPlayback();
    gifStoredOnFS=false;
    gifSize=0;
  }
  // Preserve the last logical carousel frame while a replacement bank is uploaded.
  // BUILD 96 added the physical-output blackout used during replacement.
  // Direct assignment is intentional: switchDisplayMode() would clear the
  // still-open Device Assets upload context.
  if(displayMode==DISPLAY_GIF || displayMode==DISPLAY_TEXT) displayMode=DISPLAY_RAW;
}

int8_t nextCarouselSlot(int8_t current) {
  if(!littleFsReady || !carouselOrderCount) return -1;
  int start=-1;
  for(uint8_t i=0;i<carouselOrderCount;i++) if((int8_t)carouselOrder[i]==current){ start=i; break; }
  for(uint8_t step=1; step<=carouselOrderCount; step++){
    uint8_t pos=(uint8_t)((start<0 ? step-1 : start+step)%carouselOrderCount);
    uint8_t slot=carouselOrder[pos];
    if(slot>=CAROUSEL_SLOT_COUNT) continue;
    const CarouselSlotMeta &m=carouselSlots[slot];
    if(!m.configured || !m.mediaSize || (m.dataType!=1 && m.dataType!=3)) continue;
    if(LittleFS.exists(carouselFileName(slot,m.dataType))) return (int8_t)slot;
  }
  return -1;
}

bool startCarouselSlot(uint8_t slot) {
  if(presetActive) stopPresetPlayback();
  if(!littleFsReady || slot>=CAROUSEL_SLOT_COUNT) return false;
  carouselActive=false;
  carouselActiveSlot=-1;
  CarouselSlotMeta &m=carouselSlots[slot];
  if(!m.configured || !m.mediaSize || (m.dataType!=1 && m.dataType!=3)) return false;
  String path=carouselFileName(slot,m.dataType);
  if(!LittleFS.exists(path) || !fileMatchesMedia(path,m.mediaSize,m.mediaCRC)) return false;

  bool started=false;
  stopGIFPlayback();
  if(m.dataType==1){
    gifSize=m.mediaSize;
    started=startGIFFile(path.c_str(),false,true);
    if(!started){ gifSize=0; gifStoredOnFS=false; }
  } else if(m.dataType==3){
    if(m.mediaSize<=MAX_TEXT_PAYLOAD){
      File f=LittleFS.open(path,"r");
      if(f && (uint32_t)f.size()==m.mediaSize){
        size_t got=f.read(textPayload,m.mediaSize);
        f.close();
        if(got==m.mediaSize) started=parseTextPayloadInternal(textPayload,got,true);
      } else if(f) f.close();
    }
  }
  if(!started) return false;

  carouselActive=true;
  carouselActiveSlot=(int8_t)slot;
  carouselSlotStartedAt=millis();
#if DEBUG_SERIAL && CAROUSEL_PROTOCOL_DEBUG
  Serial.print("CAROUSEL PLAY slot="); Serial.print(slot);
  Serial.print(" type="); Serial.print(m.dataType==3 ? "TEXT" : "GIF");
  Serial.print(" dwell="); Serial.print(m.dwellSeconds ? m.dwellSeconds : CAROUSEL_DEFAULT_DWELL_SEC);
  Serial.print("s bytes="); Serial.println(m.mediaSize);
#endif
  return true;
}

void updateCarousel(uint32_t now) {
  // Official-app captures show 0A/01 can establish the Assets view before a
  // later page push, with no second 0A/01 after the Bulk transfers.  Therefore
  // 02/01 preserves that view intent.  Because no explicit end-of-push frame
  // has been observed, BUILD 96 uses a conservative idle-settle boundary only
  // after at least one carousel asset has committed.  This is emulator policy,
  // not a claimed original-device protocol timing.
  if(carouselUploadOpen && carouselLastAssetCommitAt && !bulk.active &&
     (uint32_t)(now-carouselLastAssetCommitAt)>=CAROUSEL_UPLOAD_SETTLE_MS){
    carouselUploadOpen=false;
    carouselUploadBlackout=false;
    endCarouselTransferIndicator();
    bool canStart=carouselEnterRequested && nextCarouselSlot(-1)>=0;
    if(canStart) carouselStartPending=true;
    else refreshMatrix(); // restore the preserved framebuffer if Assets view is not active
#if DEBUG_SERIAL && CAROUSEL_PROTOCOL_DEBUG
    Serial.print("CAROUSEL UPLOAD SETTLED after "); Serial.print(CAROUSEL_UPLOAD_SETTLE_MS);
    Serial.println(canStart ? "ms; blackout OFF; starting stored bank" : "ms; blackout OFF; view not active");
#endif
  }

  if(carouselStartPending){
    carouselStartPending=false;
    int8_t first=nextCarouselSlot(-1);
    if(first>=0) startCarouselSlot((uint8_t)first);
    return;
  }
  if(!carouselActive || carouselActiveSlot<0) return;
  uint16_t dwell=carouselSlots[(uint8_t)carouselActiveSlot].dwellSeconds;
  if(!dwell) dwell=CAROUSEL_DEFAULT_DWELL_SEC;
  if((uint32_t)(now-carouselSlotStartedAt) < (uint32_t)dwell*1000UL) return;
  int8_t next=nextCarouselSlot(carouselActiveSlot);
  if(next>=0) startCarouselSlot((uint8_t)next);
}

bool commitCarouselSlot(uint8_t slot, uint8_t dataType, uint16_t dwellSeconds, uint32_t size, uint32_t crc) {
  if(!littleFsReady || slot>=CAROUSEL_SLOT_COUNT || !size || (dataType!=1 && dataType!=3)) return false;
  String tmp=carouselTempFileName(slot), dst=carouselFileName(slot,dataType), bak=carouselBackupFileName(slot);
  if(!fileMatchesMedia(tmp,size,crc)) return false;

  CarouselSlotMeta oldMeta=carouselSlots[slot];
  String oldDst=(oldMeta.configured && (oldMeta.dataType==1 || oldMeta.dataType==3)) ? carouselFileName(slot,oldMeta.dataType) : String();
  bool hadOld=oldDst.length() && LittleFS.exists(oldDst);
  LittleFS.remove(bak);
  if(hadOld && !LittleFS.rename(oldDst,bak)) return false;
  if(oldDst!=dst) LittleFS.remove(dst);
  if(!LittleFS.rename(tmp,dst)){
    if(hadOld) LittleFS.rename(bak,oldDst);
    return false;
  }

  CarouselSlotMeta newMeta;
  newMeta.configured=1; newMeta.dataType=dataType; newMeta.dwellSeconds=dwellSeconds; newMeta.mediaSize=size; newMeta.mediaCRC=crc;
  carouselSlots[slot]=newMeta;
  if(!saveCarouselSlotMeta(slot)){
    LittleFS.remove(dst);
    if(hadOld) LittleFS.rename(bak,oldDst);
    carouselSlots[slot]=oldMeta;
    if(oldMeta.configured) saveCarouselSlotMeta(slot); else removeCarouselSlotMeta(slot);
    return false;
  }
  LittleFS.remove(bak);
  LittleFS.remove(carouselFileName(slot,dataType==1 ? 3 : 1));
  carouselLastAssetCommitAt=millis();
#if DEBUG_SERIAL && CAROUSEL_PROTOCOL_DEBUG
  Serial.print("CAROUSEL STORE slot="); Serial.print(slot);
  Serial.print(" type="); Serial.print(dataType==3 ? "TEXT" : "GIF");
  Serial.print(" dwell="); Serial.print(dwellSeconds);
  Serial.print("s bytes="); Serial.println(size);
#endif
  return true;
}

bool handleCarouselCommand(const uint8_t *data, size_t len) {
  if(!data || len<4) return false;

  // Slot setup / material wipe. Public hardware-validated RE shows byte 4 as
  // a count followed by that many device-slot IDs. The current official app
  // sends all twelve IDs (0..11) even when only a subset receives media.
  if(data[2]==0x02 && data[3]==0x01){
    if(len<5 || ((uint16_t)data[0] | ((uint16_t)data[1]<<8))!=len) return false;
    uint8_t count=data[4];
    if(count>CAROUSEL_SLOT_COUNT || len!=(size_t)(5U+count)) return false;
    bool seen[CAROUSEL_SLOT_COUNT]={false};
    for(uint8_t i=0;i<count;i++){
      uint8_t slot=data[5+i];
      if(slot>=CAROUSEL_SLOT_COUNT || seen[slot]) return false;
      seen[slot]=true;
    }

    // A material wipe starts a replacement transaction. Captures from the
    // official app show that 0A/01 may have established the Assets view before
    // this push and that no second 0A/01 is necessarily sent afterwards. Keep
    // that view intent, but stop playback so no partially replaced bank is
    // rendered while slots are still arriving.
    bool hadAssetView=carouselEnterRequested || carouselActive || gifCarouselPlaybackFileActive;
    if(carouselActive || gifCarouselPlaybackFileActive || displayMode==DISPLAY_TEXT) stopCarouselPlayback();
    // A page push supersedes any deferred live-GIF promotion that could
    // otherwise start in the middle of the Device Assets replacement.
    if(pendingGifStart){
      if(littleFsReady && pendingGifSlot>=0 && pendingGifSlot<=1) LittleFS.remove(GIF_RX_FILES[(uint8_t)pendingGifSlot]);
      pendingGifStart=false; pendingGifSlot=-1; pendingGifSize=0; pendingGifStartAt=0;
    }
    carouselEnterRequested=hadAssetView;
    carouselStartPending=false;
    carouselUploadOpen=true;
    carouselUploadBlackout=true;
    carouselUploadCompletedMask=0;
    endCarouselTransferIndicator();
    carouselLastAssetCommitAt=0;
    carouselOrderCount=count;
    for(uint8_t i=0;i<count;i++) carouselOrder[i]=data[5+i];
    for(uint8_t i=count;i<CAROUSEL_SLOT_COUNT;i++) carouselOrder[i]=0;

    // Clear exactly the declared device slots. The app can then repopulate any
    // subset; missing/empty positions are skipped during playback.
    for(uint8_t i=0;i<count;i++) clearCarouselSlot(data[5+i]);
    saveCarouselOrder();
    // Blank the physical matrix immediately while preserving framebuffer and
    // screenOn. Every refresh during the push remains black until playback is
    // armed from updateCarousel().
    refreshMatrix();
#if DEBUG_SERIAL && CAROUSEL_PROTOCOL_DEBUG
    Serial.print("CAROUSEL SLOT SETUP count="); Serial.print(count); Serial.print(" slots=");
    for(uint8_t i=0;i<count;i++){ if(i) Serial.print(','); Serial.print(data[5+i]); }
    Serial.print(" playback=SUSPENDED matrix=BLACK viewIntent=");
    Serial.println(carouselEnterRequested ? "YES" : "NO");
#endif
    sendCommandAck(0x02,0x01);
    return true;
  }

  // Enter/start Device Assets view. Captures show this may arrive before a
  // subsequent page push, not necessarily after it. Therefore it establishes
  // persistent view intent; an open replacement upload is allowed to finish
  // before playback starts. Repeated 0A/01 while already playing is idempotent.
  if(len==4 && data[2]==0x0A && data[3]==0x01){
    carouselEnterRequested=true;
    if(!carouselUploadOpen && !carouselActive && nextCarouselSlot(-1)>=0) carouselStartPending=true;
#if DEBUG_SERIAL && CAROUSEL_PROTOCOL_DEBUG
    Serial.print("CAROUSEL ASSET VIEW (0A/01) uploadOpen=");
    Serial.print(carouselUploadOpen ? "YES" : "NO");
    Serial.print(" active="); Serial.println(carouselActive ? "YES" : "NO");
#endif
    sendCommandAck(0x0A,0x01);
    return true;
  }
  return false;
}

// ======================================================
// PRESET / DEFAULT volatile bank
// ======================================================
String presetFileName(uint8_t localSlot, uint8_t dataType) {
  return String("/pre") + localSlot + (dataType==3 ? ".txt" : ".gif");
}
String presetTempFileName(uint8_t localSlot) { return String("/pre") + localSlot + ".tmp"; }

void clearPresetSlot(uint8_t localSlot) {
  if(localSlot>=PRESET_SLOT_COUNT) return;
  if(littleFsReady){
    LittleFS.remove(presetTempFileName(localSlot));
    LittleFS.remove(presetFileName(localSlot,1));
    LittleFS.remove(presetFileName(localSlot,3));
  }
  presetSlots[localSlot]=PresetSlotMeta();
}

void clearPresetBank(bool removeFiles) {
  presetActive=false;
  presetActiveSlot=-1;
  presetOrderCount=0;
  presetSlotHoldMs=PRESET_DWELL_MS;
  gifPresetPlaybackFileActive=false;
  for(uint8_t i=0;i<PRESET_SLOT_COUNT;i++){
    presetOrder[i]=0;
    if(removeFiles) clearPresetSlot(i);
    else presetSlots[i]=PresetSlotMeta();
  }
}

void stopPresetPlayback() {
  const bool hadGif=gifPresetPlaybackFileActive;
  presetActive=false;
  presetActiveSlot=-1;
  presetSlotHoldMs=PRESET_DWELL_MS;
  gifPresetPlaybackFileActive=false;
  if(hadGif) stopGIFPlayback();
  if(displayMode==DISPLAY_GIF || displayMode==DISPLAY_TEXT) displayMode=DISPLAY_RAW;
}

bool commitPresetSlot(uint8_t localSlot, uint8_t dataType, uint16_t timeSign, uint32_t size, uint32_t crc) {
  if(!littleFsReady || localSlot>=PRESET_SLOT_COUNT || !size || (dataType!=1 && dataType!=3)) return false;
  String tmp=presetTempFileName(localSlot);
  if(!fileMatchesMedia(tmp,size,crc)) return false;

  // Never replace the file currently being decoded/rendered.
  if(presetActive && presetActiveSlot==(int8_t)localSlot) stopPresetPlayback();

  String dst=presetFileName(localSlot,dataType);
  LittleFS.remove(dst);
  if(!LittleFS.rename(tmp,dst)) return false;
  LittleFS.remove(presetFileName(localSlot,dataType==1 ? 3 : 1));

  PresetSlotMeta &m=presetSlots[localSlot];
  m.configured=1;
  m.dataType=dataType;
  m.timeSign=timeSign;
  m.mediaSize=size;
  m.mediaCRC=crc;
#if DEBUG_SERIAL && PRESET_PROTOCOL_DEBUG
  Serial.print("PRESET STORE slot="); Serial.print((unsigned)(PRESET_SLOT_BASE+localSlot));
  Serial.print(" type="); Serial.print(dataType==3 ? "TEXT" : "GIF");
  Serial.print(" timeSign="); Serial.print(timeSign);
  Serial.print(" bytes="); Serial.println(size);
#endif
  return true;
}

int8_t nextPresetSlot(int8_t currentLocal) {
  if(!littleFsReady || !presetOrderCount) return -1;
  int start=-1;
  for(uint8_t i=0;i<presetOrderCount;i++) if((int8_t)presetOrder[i]==currentLocal){ start=i; break; }
  for(uint8_t step=1;step<=presetOrderCount;step++){
    uint8_t pos=(uint8_t)((start<0 ? step-1 : start+step)%presetOrderCount);
    uint8_t local=presetOrder[pos];
    if(local>=PRESET_SLOT_COUNT) continue;
    const PresetSlotMeta &m=presetSlots[local];
    if(!m.configured || !m.mediaSize || (m.dataType!=1 && m.dataType!=3)) continue;
    String path=presetFileName(local,m.dataType);
    if(fileMatchesMedia(path,m.mediaSize,m.mediaCRC)) return (int8_t)local;
  }
  return -1;
}

uint32_t presetTextHoldDurationMs() {
  if(!textState.valid || !textState.glyphCount) return PRESET_DWELL_MS;

  const uint8_t motion=textState.motionEffect;
  const uint8_t capacity=textVisibleGlyphCapacity();
  const uint16_t pages=(capacity>0)
      ? (uint16_t)((textState.glyphCount + capacity - 1U) / capacity)
      : 1U;

  // LEFT/RIGHT are true continuous scroll modes. The original display advances
  // the Preset as soon as the final glyph has completely left the matrix.
  if(motion==1 || motion==2){
    const uint32_t textWidth=(uint32_t)textState.glyphCount * textState.glyphAdvance;
    const uint32_t travel=(uint32_t)MATRIX_WIDTH + textWidth + 1U;
    return max((uint32_t)textFrameInterval(), travel * (uint32_t)textFrameInterval());
  }

  // PIN and the page/viewport based modes must show every page once, then keep
  // the final page visible for the same ~3 s tail observed on original hardware.
  if(motion==3 || motion==4){
    const uint32_t pageTravel=(uint32_t)textState.glyphHeight + 1U;
    const uint32_t perPage=pageTravel * (uint32_t)textFrameInterval();
    return (pages>1U ? (uint32_t)(pages-1U)*perPage : 0U) + PRESET_DWELL_MS;
  }

  return (pages>1U ? (uint32_t)(pages-1U)*(uint32_t)textPageInterval() : 0U)
       + PRESET_DWELL_MS;
}

bool startPresetSlot(uint8_t localSlot) {
  if(!littleFsReady || localSlot>=PRESET_SLOT_COUNT) return false;
  PresetSlotMeta &m=presetSlots[localSlot];
  if(!m.configured || !m.mediaSize || (m.dataType!=1 && m.dataType!=3)) return false;
  String path=presetFileName(localSlot,m.dataType);
  if(!fileMatchesMedia(path,m.mediaSize,m.mediaCRC)) return false;

  if(carouselActive || gifCarouselPlaybackFileActive || carouselStartPending) stopCarouselPlayback();
  stopGIFPlayback();
  gifPresetPlaybackFileActive=false;

  bool started=false;
  if(m.dataType==1){
    gifSize=m.mediaSize;
    started=startGIFFile(path.c_str(),false,false);
    if(started) gifPresetPlaybackFileActive=true;
    else { gifSize=0; gifStoredOnFS=false; }
  } else {
    if(m.mediaSize<=MAX_TEXT_PAYLOAD){
      File f=LittleFS.open(path,"r");
      if(f && (uint32_t)f.size()==m.mediaSize){
        size_t got=f.read(textPayload,m.mediaSize);
        f.close();
        if(got==m.mediaSize) started=parseTextPayloadInternal(textPayload,got,true);
      } else if(f) f.close();
    }
  }
  if(!started) return false;

  presetActive=true;
  presetActiveSlot=(int8_t)localSlot;
  presetSlotStartedAt=millis();
  presetSlotHoldMs=(m.dataType==3) ? presetTextHoldDurationMs() : PRESET_DWELL_MS;
#if DEBUG_SERIAL && PRESET_PROTOCOL_DEBUG
  Serial.print("PRESET PLAY slot="); Serial.print((unsigned)(PRESET_SLOT_BASE+localSlot));
  Serial.print(" type="); Serial.print(m.dataType==3 ? "TEXT" : "GIF");
  Serial.print(" holdMs="); Serial.print(presetSlotHoldMs);
  if(m.dataType==3){
    Serial.print(" motion="); Serial.print(textState.motionEffect);
    Serial.print(" pages=");
    const uint8_t cap=textVisibleGlyphCapacity();
    Serial.print(cap ? (unsigned)((textState.glyphCount+cap-1U)/cap) : 1U);
  }
  Serial.println();
#endif
  return true;
}

void updatePreset(uint32_t now) {
  if(!presetActive || presetActiveSlot<0 || presetOrderCount<=1) return;
  if((uint32_t)(now-presetSlotStartedAt)<presetSlotHoldMs) return;
  int8_t next=nextPresetSlot(presetActiveSlot);
  if(next>=0 && next!=presetActiveSlot) startPresetSlot((uint8_t)next);
  else presetSlotStartedAt=now;
}

bool handlePresetCommand(const uint8_t *data, size_t len) {
  if(!data || len<5 || data[2]!=0x06 || data[3]!=0x02) return false;
  uint16_t declared=(uint16_t)data[0] | ((uint16_t)data[1]<<8);
  if(declared!=len) return false;
  uint8_t count=data[4];
  if(count>PRESET_SLOT_COUNT || len!=(size_t)(5U+count)) return false;

  bool seen[PRESET_SLOT_COUNT]={false};
  uint8_t localOrder[PRESET_SLOT_COUNT]={0};
  for(uint8_t i=0;i<count;i++){
    uint8_t deviceSlot=data[5+i];
    if(deviceSlot<PRESET_SLOT_BASE || deviceSlot>=PRESET_SLOT_BASE+PRESET_SLOT_COUNT) return false;
    uint8_t local=(uint8_t)(deviceSlot-PRESET_SLOT_BASE);
    if(seen[local]) return false;
    seen[local]=true;
    localOrder[i]=local;
  }

#if DEBUG_SERIAL && PRESET_PROTOCOL_DEBUG
  Serial.print("PRESET ACT count="); Serial.print(count); Serial.print(" slots=");
  for(uint8_t i=0;i<count;i++){ if(i) Serial.print(','); Serial.print(data[5+i]); }
  Serial.println();
#endif

  stopPresetPlayback();
  presetOrderCount=count;
  for(uint8_t i=0;i<count;i++) presetOrder[i]=localOrder[i];
  for(uint8_t i=count;i<PRESET_SLOT_COUNT;i++) presetOrder[i]=0;

  if(count){
    int8_t first=nextPresetSlot(-1);
    if(first>=0) startPresetSlot((uint8_t)first);
#if DEBUG_SERIAL && PRESET_PROTOCOL_DEBUG
    else Serial.println("PRESET ACT: no valid uploaded slots");
#endif
  }
  sendCommandAck(0x06,0x02);
  return true;
}

// ======================================================
// BULK
// ======================================================
bool startsWithGIF(const uint8_t *data,size_t len){ return len>=6 && (!memcmp(data,"GIF87a",6)||!memcmp(data,"GIF89a",6)); }
void resetBulkTransfer(bool removePartialGif=false){
  int8_t rxSlot=bulk.gifRxSlot;
  const bool abortedCarouselTransfer=removePartialGif && bulk.carouselToFS;
  if(gifBulkFile) gifBulkFile.close();
  if(littleFsReady && removePartialGif){
    if(bulk.carouselToFS && bulk.carouselLocalSlot>=0 && bulk.carouselLocalSlot<CAROUSEL_SLOT_COUNT){
      LittleFS.remove(carouselTempFileName((uint8_t)bulk.carouselLocalSlot));
    } else if(bulk.presetToFS && bulk.presetLocalSlot>=0 && bulk.presetLocalSlot<PRESET_SLOT_COUNT){
      LittleFS.remove(presetTempFileName((uint8_t)bulk.presetLocalSlot));
    } else if(rxSlot>=0 && rxSlot<=1){
      // Never remove a completed file already queued for deferred playback.
      if(!(pendingGifStart && pendingGifSlot==rxSlot)) LittleFS.remove(GIF_RX_FILES[(uint8_t)rxSlot]);
    }
  }
  bulk=BulkTransferState();
  bulkLastRxMs=0;
  textPayloadReceived=0;
  if(rawRgbData){ free(rawRgbData); rawRgbData=nullptr; }
  rawRgbWriteOffset=0;
  if(abortedCarouselTransfer) endCarouselTransferIndicator();
}

void expireStalledTransfers(uint32_t now){
  // These are emulator safety timeouts, not inferred protocol timings.
  if(packetReceived && packetLastRxMs && (uint32_t)(now-packetLastRxMs)>PACKET_REASSEMBLY_TIMEOUT_MS){
#if DEBUG_SERIAL
    Serial.println("PACKET RX timeout; dropping incomplete logical packet");
#endif
    packetReceived=packetExpected=0;
    packetLastRxMs=0;
    resetBulkTransfer(true);
  }
  if(bulk.active && bulkLastRxMs && (uint32_t)(now-bulkLastRxMs)>BULK_TRANSFER_TIMEOUT_MS){
#if DEBUG_SERIAL
    Serial.println("BULK timeout; aborting stalled transfer");
#endif
    resetBulkTransfer(true);
  }
}

bool processBulkPacket(const uint8_t *data,size_t len){
  if(len<16 || data[3]!=0x00 || data[2]>0x03) return false;
  uint8_t type=data[2];
  uint32_t total=(uint32_t)data[5]|((uint32_t)data[6]<<8)|((uint32_t)data[7]<<16)|((uint32_t)data[8]<<24);
  uint32_t crc=(uint32_t)data[9]|((uint32_t)data[10]<<8)|((uint32_t)data[11]<<16)|((uint32_t)data[12]<<24);
  uint16_t timeSign=(uint16_t)data[13]|((uint16_t)data[14]<<8);
  uint8_t imageIndex=data[15];
  const bool presetTraceSlot=(imageIndex>=0x0E && imageIndex<=0x13);
#if DEBUG_SERIAL && PRESET_PROTOCOL_DEBUG
  if((type==1 || type==3) && presetTraceSlot){
    Serial.print("PRESET BULK HDR type="); Serial.print(type);
    Serial.print(" slot="); Serial.print(imageIndex);
    Serial.print(" timeSign="); Serial.print(timeSign);
    Serial.print(" total="); Serial.print(total);
    Serial.print(" crc=0x"); Serial.println(crc,HEX);
    Serial.print("PRESET BULK RAW [16]: "); dumpHex(data,16);
  }
#endif
  if(!total || total>10UL*1024UL*1024UL) return false;
  if(type==3 && total>MAX_TEXT_PAYLOAD){
#if DEBUG_SERIAL
    Serial.print("TEXT RX rejected: declared payload "); Serial.print(total);
    Serial.print(" exceeds MAX_TEXT_PAYLOAD="); Serial.println(MAX_TEXT_PAYLOAD);
#endif
    resetBulkTransfer(true);
    sendTransferAck(type,0x03);
    return true;
  }
  const uint8_t *payload=data+16; size_t payloadSize=len-16;
  if(!bulk.active){
    resetBulkTransfer(); bulk.active=true; bulk.dataType=type; bulk.expectedSize=total; bulk.expectedCRC=crc; bulk.runningCRC=0xFFFFFFFF;
    bulk.timeSign=timeSign; bulk.imageIndex=imageIndex;
    if(startsWithGIF(payload,payloadSize)) bulk.format="GIF";
    else if(type==2&&total==(uint32_t)NUM_LEDS*3UL) bulk.format="RAW RGB";
    else if(type==3) bulk.format="TEXT";
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
    Serial.print("BULK BEGIN type="); Serial.print(type);
    Serial.print(" total="); Serial.print(total);
    Serial.print(" firstPayload="); Serial.print(payloadSize);
    if(type==1 || type==3){ Serial.print(" timeSign="); Serial.print(timeSign); Serial.print(" imageIndex="); Serial.print(imageIndex); }
    Serial.print(" format="); Serial.println(bulk.format.length() ? bulk.format : "UNKNOWN");
#endif
    if((type==1 || type==3) && presetTraceSlot){
      // A new preset invocation re-sends the whole selected bank. Freeze the
      // previous preset on its last framebuffer while replacement assets arrive
      // so old/new files can never be mixed during a long multi-chunk upload.
      if(presetActive) stopPresetPlayback();
      if(!littleFsReady){
#if DEBUG_SERIAL
        Serial.println("PRESET BULK rejected: LittleFS unavailable");
#endif
        resetBulkTransfer(true); sendTransferAck(type,0x03); return true;
      }
      size_t fsTotal=LittleFS.totalBytes();
      size_t fsUsed=LittleFS.usedBytes();
      size_t fsFree=fsTotal>fsUsed ? fsTotal-fsUsed : 0;
      if((uint64_t)total > (uint64_t)fsFree){
#if DEBUG_SERIAL
        Serial.print("PRESET BULK rejected: bytes="); Serial.print(total);
        Serial.print(" LittleFS free="); Serial.println(fsFree);
#endif
        resetBulkTransfer(true); sendTransferAck(type,0x03); return true;
      }
      uint8_t localSlot=(uint8_t)(imageIndex-PRESET_SLOT_BASE);
      bulk.presetLocalSlot=(int8_t)localSlot;
      bulk.presetToFS=true;
      String tmp=presetTempFileName(localSlot);
      LittleFS.remove(tmp);
      gifBulkFile=LittleFS.open(tmp,"w");
      if(!gifBulkFile){ resetBulkTransfer(true); sendTransferAck(type,0x03); return true; }
      gifRxWriteOffset=0;
#if DEBUG_SERIAL && PRESET_PROTOCOL_DEBUG
      Serial.print("PRESET RX BEGIN slot="); Serial.print(imageIndex);
      Serial.print(" type="); Serial.print(type);
      Serial.print(" timeSign="); Serial.println(timeSign);
#endif
    }
    if(type==3 && !bulk.presetToFS && carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT){
      if(!littleFsReady){
#if DEBUG_SERIAL
        Serial.println("BULK TEXT carousel rejected: LittleFS unavailable");
#endif
        resetBulkTransfer(true); sendTransferAck(type,0x03); return true;
      }
      size_t fsTotal=LittleFS.totalBytes();
      size_t fsUsed=LittleFS.usedBytes();
      size_t fsFree=fsTotal>fsUsed ? fsTotal-fsUsed : 0;
      if((uint64_t)total > (uint64_t)fsFree){
#if DEBUG_SERIAL
        Serial.print("BULK TEXT carousel rejected: bytes="); Serial.print(total);
        Serial.print(" LittleFS free="); Serial.println(fsFree);
#endif
        resetBulkTransfer(true); sendTransferAck(type,0x03); return true;
      }
      uint8_t localSlot=imageIndex;
      bulk.carouselLocalSlot=(int8_t)localSlot;
      String tmp=carouselTempFileName(localSlot);
      LittleFS.remove(tmp);
      gifBulkFile=LittleFS.open(tmp,"w");
      if(!gifBulkFile){ resetBulkTransfer(true); sendTransferAck(type,0x03); return true; }
      bulk.carouselToFS=true;
      gifRxWriteOffset=0;
      beginCarouselTransferIndicator(total,localSlot);
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
      Serial.print("BULK TEXT STORAGE: CAROUSEL slot="); Serial.print(localSlot);
      Serial.print(" dwell="); Serial.println(timeSign);
#endif
    }
    if(type==1 && !bulk.presetToFS){
      if(!littleFsReady){
#if DEBUG_SERIAL
        Serial.println("BULK GIF rejected: LittleFS unavailable");
#endif
        resetBulkTransfer(true); sendTransferAck(type,0x03); return true;
      }
      size_t fsTotal=LittleFS.totalBytes();
      size_t fsUsed=LittleFS.usedBytes();
      size_t fsFree=fsTotal>fsUsed ? fsTotal-fsUsed : 0;
      if((uint64_t)total > (uint64_t)fsFree){
#if DEBUG_SERIAL
        Serial.print("BULK GIF rejected: bytes="); Serial.print(total);
        Serial.print(" LittleFS free="); Serial.println(fsFree);
#endif
        resetBulkTransfer(true); sendTransferAck(type,0x03); return true;
      }

      if(carouselUploadOpen && imageIndex<CAROUSEL_SLOT_COUNT){
        // Device Assets use imageIndex directly as persistent slot 0..11.
        uint8_t localSlot=imageIndex;
        // Never replace a file while AnimatedGIF is decoding that same slot.
        // Preserve Assets-view intent so playback resumes after commit.
        if(carouselActive && carouselActiveSlot==(int8_t)localSlot) stopCarouselPlayback();
        bulk.carouselLocalSlot=(int8_t)localSlot;
        String tmp=carouselTempFileName(localSlot);
        LittleFS.remove(tmp);
        gifBulkFile=LittleFS.open(tmp,"w");
        if(!gifBulkFile){ resetBulkTransfer(true); sendTransferAck(type,0x03); return true; }
        bulk.carouselToFS=true;
        gifRxWriteOffset=0;
        beginCarouselTransferIndicator(total,localSlot);
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
        Serial.print("BULK GIF STORAGE: CAROUSEL slot="); Serial.print(localSlot);
        Serial.print(" dwell="); Serial.println(timeSign);
#endif
      } else {
        // Live/preview GIF path: preserve the stable alternating RX -> PLAY flow.
        int8_t slot=(int8_t)(nextGifRxSlot & 1U);
        nextGifRxSlot ^= 1U;
        if(pendingGifStart && slot==pendingGifSlot) slot ^= 1;
        bulk.gifRxSlot=slot;
        const char *rxPath=GIF_RX_FILES[(uint8_t)slot];
        LittleFS.remove(rxPath);
        gifBulkFile=LittleFS.open(rxPath,"w");
        if(!gifBulkFile){ resetBulkTransfer(true); sendTransferAck(type,0x03); return true; }
        bulk.gifToFS=true;
        gifRxWriteOffset=0;
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
        Serial.print("BULK GIF STORAGE: LittleFS RX slot="); Serial.print(slot);
        Serial.print(" imageIndex="); Serial.println(imageIndex);
#endif
      }
    }
    if(type==2 && bulk.format=="RAW RGB"){
      rawRgbData=(uint8_t*)malloc(total);
      if(!rawRgbData){ resetBulkTransfer(true); sendTransferAck(type,0x03); return true; }
      rawRgbWriteOffset=0;
    }
  }
  bulkLastRxMs=millis();
  if(type!=bulk.dataType || total!=bulk.expectedSize || crc!=bulk.expectedCRC ||
     ((type==1 || bulk.carouselToFS || bulk.presetToFS) && (timeSign!=bulk.timeSign || imageIndex!=bulk.imageIndex))){
#if DEBUG_SERIAL
    Serial.println("BULK header mismatch; aborting transaction");
#endif
    resetBulkTransfer(true); return true;
  }
  uint32_t remain=bulk.expectedSize-bulk.receivedSize; size_t useful=min((size_t)remain,payloadSize);
  bulk.runningCRC=crc32Update(bulk.runningCRC,payload,useful);
  if(bulk.carouselToFS || bulk.presetToFS || (type==1 && bulk.gifToFS)){
    size_t n=gifBulkFile ? gifBulkFile.write(payload,useful) : 0;
    gifRxWriteOffset+=n;
    if(n!=useful){
#if DEBUG_SERIAL
      Serial.println("BULK asset LittleFS RX write error");
#endif
      resetBulkTransfer(true); sendTransferAck(type,0x03); return true;
    }
  }
  if(type==2 && rawRgbData && rawRgbWriteOffset+useful<=bulk.expectedSize){
    ::memcpy(rawRgbData+rawRgbWriteOffset,payload,useful);
    rawRgbWriteOffset+=useful;
  }
  if(type==3 && !bulk.carouselToFS && !bulk.presetToFS){
    size_t off=bulk.receivedSize;
    if(off<MAX_TEXT_PAYLOAD){ size_t cp=min(useful,(size_t)MAX_TEXT_PAYLOAD-off); ::memcpy(textPayload+off,payload,cp); textPayloadReceived=off+cp; }
  }
  bulk.receivedSize+=useful; bulk.chunkCount++;
  if(bulk.carouselToFS) updateCarouselTransferIndicatorProgress(bulk.receivedSize,bulk.expectedSize);
  if(bulk.receivedSize>=bulk.expectedSize){
    bool ok=((bulk.runningCRC^0xFFFFFFFF)==bulk.expectedCRC);
#if DEBUG_SERIAL && TEXT_PROTOCOL_DEBUG
    if(type==3){
      Serial.print("TEXT RX COMPLETE len="); Serial.print(bulk.carouselToFS ? bulk.receivedSize : textPayloadReceived);
      Serial.print(" carousel="); Serial.print(bulk.carouselToFS ? "YES" : "NO");
      Serial.print(" crc="); Serial.println(ok ? "OK" : "BAD");
    }
#endif
    if(bulk.carouselToFS){
      if(gifBulkFile){ gifBulkFile.flush(); gifBulkFile.close(); }
      uint8_t localSlot=(bulk.carouselLocalSlot>=0) ? (uint8_t)bulk.carouselLocalSlot : 0xFF;
      bool stored=ok && localSlot<CAROUSEL_SLOT_COUNT && gifRxWriteOffset==bulk.expectedSize &&
                  commitCarouselSlot(localSlot,type,bulk.timeSign,bulk.expectedSize,bulk.expectedCRC);
      if(!stored && localSlot<CAROUSEL_SLOT_COUNT) LittleFS.remove(carouselTempFileName(localSlot));
      if(stored && localSlot<CAROUSEL_SLOT_COUNT) carouselUploadCompletedMask |= (uint16_t)(1U<<localSlot);
      ok=ok && stored;
    } else if(bulk.presetToFS){
      if(gifBulkFile){ gifBulkFile.flush(); gifBulkFile.close(); }
      uint8_t localSlot=(bulk.presetLocalSlot>=0) ? (uint8_t)bulk.presetLocalSlot : 0xFF;
      bool stored=ok && localSlot<PRESET_SLOT_COUNT && gifRxWriteOffset==bulk.expectedSize &&
                  commitPresetSlot(localSlot,type,bulk.timeSign,bulk.expectedSize,bulk.expectedCRC);
      if(!stored && localSlot<PRESET_SLOT_COUNT) LittleFS.remove(presetTempFileName(localSlot));
      ok=ok && stored;
    } else {
#if DEBUG_SERIAL && PRESET_PROTOCOL_DEBUG
      if(type==3 && ok && textPayloadReceived && presetTraceSlot){
        const size_t h=min(textPayloadReceived,(size_t)TEXT_GLOBAL_HEADER);
        Serial.print("PRESET TEXT GLOBAL ["); Serial.print(h); Serial.print("]: "); dumpHex(textPayload,h);
        if(textPayloadReceived>TEXT_GLOBAL_HEADER){
          const size_t m=min(textPayloadReceived-(size_t)TEXT_GLOBAL_HEADER,(size_t)TEXT_GLYPH_META);
          Serial.print("PRESET TEXT META0 ["); Serial.print(m); Serial.print("]: "); dumpHex(textPayload+TEXT_GLOBAL_HEADER,m);
        }
      }
#endif
      if(type==3 && ok && textPayloadReceived) parseTextPayload(textPayload,textPayloadReceived);
      if(type==1){
        if(gifBulkFile){ gifBulkFile.flush(); gifBulkFile.close(); }
        if(ok && bulk.gifToFS && gifRxWriteOffset==bulk.expectedSize && bulk.gifRxSlot>=0){
          // Publish only metadata here. No AnimatedGIF calls, no closing the
          // current decoder and no PLAY-file mutation are allowed from Core 0.
          pendingGifSlot=bulk.gifRxSlot;
          pendingGifSize=bulk.expectedSize;
          pendingGifStartAt=millis();
          pendingGifStart=true;
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
          Serial.print("GIF RX COMPLETE queued slot="); Serial.print((int)bulk.gifRxSlot);
          Serial.print(" bytes="); Serial.println(bulk.expectedSize);
#endif
        } else {
          if(bulk.gifRxSlot>=0) LittleFS.remove(GIF_RX_FILES[(uint8_t)bulk.gifRxSlot]);
        }
      }
    }
    if(type==2 && ok && rawRgbData && rawRgbWriteOffset==bulk.expectedSize){
      switchDisplayMode(DISPLAY_RAW);
      for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++){
        const uint16_t pix=(uint16_t)y*MATRIX_WIDTH+x;
        const uint32_t o=(uint32_t)pix*3UL;
        framebuffer[logicalIndex(x,y)]=CRGB(rawRgbData[o],rawRgbData[o+1],rawRgbData[o+2]);
      }
      refreshMatrix();
    }
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
    Serial.print("BULK END type="); Serial.print(type);
    Serial.print(" total="); Serial.print(bulk.receivedSize);
    Serial.print(" chunks="); Serial.print(bulk.chunkCount);
    if(bulk.carouselToFS){
      Serial.print(" storage=CAROUSEL slot="); Serial.print((int)bulk.carouselLocalSlot);
      Serial.print(" type="); Serial.print(type==3 ? "TEXT" : "GIF");
      Serial.print(" dwell="); Serial.print(bulk.timeSign);
    } else if(bulk.presetToFS){
      Serial.print(" storage=PRESET slot="); Serial.print((int)(PRESET_SLOT_BASE+bulk.presetLocalSlot));
      Serial.print(" type="); Serial.print(type==3 ? "TEXT" : "GIF");
      Serial.print(" timeSign="); Serial.print(bulk.timeSign);
    } else if(type==1){
      Serial.print(" storage=LittleFS-RX"); Serial.print(" slot="); Serial.print((int)bulk.gifRxSlot); Serial.print(" imageIndex="); Serial.print(bulk.imageIndex);
    }
    Serial.print(" crc="); Serial.println(ok ? "OK" : "BAD");
#endif
    sendTransferAck(type,0x03); resetBulkTransfer();
  } else sendTransferAck(type,0x01);
  return true;
}

// ======================================================
// AUDIO / RHYTHM RENDERER
// ======================================================
static uint32_t audioLastLogMs = 0;
static int audioLastUiEffect = -1;

static CRGB audioRainbow(uint8_t pos, uint8_t value = 255) {
  return CHSV(pos, 255, value);
}

static void audioPixel(int16_t x, int16_t y, const CRGB &c) {
  if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) putPixel(x, y, c);
}

static void drawLineAudio(int x0,int y0,int x1,int y1,const CRGB &c) {
  int dx=abs(x1-x0), sx=x0<x1?1:-1;
  int dy=-abs(y1-y0), sy=y0<y1?1:-1;
  int err=dx+dy;
  while(true){
    audioPixel(x0,y0,c);
    if(x0==x1 && y0==y1) break;
    int e2=2*err;
    if(e2>=dy){ err+=dy; x0+=sx; }
    if(e2<=dx){ err+=dx; y0+=sy; }
  }
}

static void drawAudioHeart(int16_t ox, int16_t oy, uint8_t scale, const CRGB &outline, const CRGB &fill) {
  static const uint16_t rows[8] = {
    0b001101100, 0b011111110, 0b111111111, 0b111111111,
    0b011111110, 0b001111100, 0b000111000, 0b000010000
  };
  for (uint8_t y=0; y<8; y++) {
    for (uint8_t x=0; x<9; x++) {
      if (!(rows[y] & (1 << (8-x)))) continue;
      bool edge = false;
      if (x==0 || x==8 || y==0 || y==7) edge=true;
      else {
        bool l = rows[y] & (1 << (8-(x-1)));
        bool r = rows[y] & (1 << (8-(x+1)));
        bool u = y ? (rows[y-1] & (1 << (8-x))) : false;
        bool d = y<7 ? (rows[y+1] & (1 << (8-x))) : false;
        edge = !(l && r && u && d);
      }
      audioPixel(ox+x, oy+y, edge ? outline : fill);
      if (scale && !edge && x>1 && x<7 && y>1 && y<6)
        audioPixel(ox+x, oy+y, fill + CRGB(40,40,40));
    }
  }
}

// LEVEL 5 - red/cyan robot face seen in the app.
static void renderAudioRobotFace(uint8_t level) {
  clearFramebuffer();
  const CRGB red(255,20,25), cyan(0,235,255), blue(15,55,255), darkBlue(4,15,90);

  // Two square red eyes with cyan pupils.
  for(int y=3;y<=7;y++) {
    for(int x=1;x<=5;x++) audioPixel(x,y,red);
    for(int x=10;x<=14;x++) audioPixel(x,y,red);
  }
  for(int y=4;y<=6;y++) {
    for(int x=2;x<=3;x++) audioPixel(x,y,cyan);
    for(int x=12;x<=13;x++) audioPixel(x,y,cyan);
  }

  // Pupils drift one pixel with the beat.
  int drift=(level>=5)?1:0;
  audioPixel(3+drift,5,CRGB::White);
  audioPixel(12+drift,5,CRGB::White);

  // Blue mouth: two horizontal rows, height reacts slightly to level.
  for(int x=3;x<=12;x++) audioPixel(x,11,blue);
  for(int x=4;x<=11;x++) audioPixel(x,12,(level>=5)?blue:darkBlue);
  if(level>=7) for(int x=5;x<=10;x++) audioPixel(x,13,blue);
}

// LEVEL 4 - purple face with moving eyes and two lips that open/expand.
static void renderAudioPurpleFace(uint8_t level) {
  clearFramebuffer();
  const CRGB purple(125,20,225), purple2(82,8,165);
  const CRGB skin(248,235,210), iris(30,25,65), lip(255,35,55), lipHi(255,115,125);

  // Hair/head silhouette, deliberately irregular like the original sprite.
  for(int y=1;y<=13;y++) for(int x=1;x<=14;x++) {
    if((y==1 && (x<4||x>11)) || (y==2 && (x<2||x>13))) continue;
    if((x==1||x==14) && (y<4||y>11)) continue;
    audioPixel(x,y, ((x+y)&1)?purple:purple2);
  }

  // Pale eye blocks.
  for(int y=5;y<=8;y++) {
    for(int x=3;x<=6;x++) audioPixel(x,y,skin);
    for(int x=9;x<=12;x++) audioPixel(x,y,skin);
  }
  int eyeShift = (int)((millis()/180 + level) % 3) - 1;
  audioPixel(5+eyeShift,7,iris);
  audioPixel(10+eyeShift,7,iris);

  // Two lips: the opening widens with level, rather than a sliding line.
  uint8_t open = constrain((int)level,1,7);
  int halfW = 2 + open/2;                 // 2..5 pixels each side
  int cx=8;
  for(int dx=-halfW; dx<=halfW; dx++) {
    int taper=abs(dx);
    if(taper<=halfW) audioPixel(cx+dx,10 + (taper>halfW-2), lipHi);
    if(taper<=halfW) audioPixel(cx+dx,12 - (taper>halfW-2), lip);
  }
  // Dark opening between the lips grows vertically on louder beats.
  for(int x=cx-halfW+1;x<=cx+halfW-1;x++) audioPixel(x,11,CRGB(35,0,25));
  if(open>=6) for(int x=cx-halfW+2;x<=cx+halfW-2;x++) audioPixel(x,12,CRGB(35,0,25));
}

// LEVEL 2 - retained because it was visually accepted.
static void renderAudioHeartLevel(uint8_t level) {
  clearFramebuffer();
  uint8_t hot = (uint8_t)min(255, 80 + (int)level*24);
  drawAudioHeart(3,4,level>=5,CRGB::White,CRGB(hot,0,20));
  if(level>=6) { audioPixel(1,7,CRGB(255,0,30)); audioPixel(14,7,CRGB(255,0,30)); }
}

// LEVEL 1 - multi-pose breakdance sprite: head, hands and feet all move.
static void renderAudioBreakdancer(uint8_t level) {
  clearFramebuffer();
  const CRGB white=CRGB::White;
  uint8_t pose=(uint8_t)((millis()/150 + level) % 6);

  // Top checker strip from the original preview, shifted with the pose.
  const CRGB green(45,255,25), green2(15,150,15);
  for(int x=0;x<16;x++) if(((x+pose)&1)==0){ audioPixel(x,0,green); audioPixel(x,1,green2); }

  // Pose definitions: head, shoulder, hip, two hands, two feet.
  struct P { int hx,hy,sx,sy,px,py,lx,ly,rx,ry,lfx,lfy,rfx,rfy; };
  static const P p[6]={
    {8,3,8,6,8,10, 4,7,12,8, 5,14,11,14},
    {7,3,8,6,8,10, 3,9,12,5, 4,13,12,14},
    {9,4,8,7,7,10, 3,5,13,10, 2,14,10,13},
    {5,7,7,8,9,10, 3,11,10,5, 4,14,13,12},
    {8,11,8,9,8,7, 4,12,12,12, 5,4,11,4}, // inverted/headstand-like pose
    {10,3,9,6,8,10, 5,5,13,7, 4,14,10,13}
  };
  const P &q=p[pose];

  // 3x3 head with pose-dependent tilt.
  for(int yy=-1;yy<=1;yy++) for(int xx=-1;xx<=1;xx++) audioPixel(q.hx+xx,q.hy+yy,white);
  // torso, arms and legs.
  drawLineAudio(q.sx,q.sy,q.px,q.py,white);
  drawLineAudio(q.sx,q.sy,q.lx,q.ly,white);
  drawLineAudio(q.sx,q.sy,q.rx,q.ry,white);
  drawLineAudio(q.px,q.py,q.lfx,q.lfy,white);
  drawLineAudio(q.px,q.py,q.rfx,q.rfy,white);

  // hands/feet are emphasized because the original breakdance frames read through extremities.
  audioPixel(q.lx,q.ly,white); audioPixel(q.rx,q.ry,white);
  audioPixel(q.lfx,q.lfy,white); audioPixel(q.rfx,q.rfy,white);
}

static uint8_t bandHeight(uint8_t v) { return constrain((int)v, 0, 12); }

// FFT 0 - already validated by the user.
static void renderAudioFFT0(const uint8_t *b) {
  clearFramebuffer();
  for(uint8_t x=0;x<16;x++) {
    uint8_t v=bandHeight(b[x<8?x:15-x]);
    uint8_t half=(v+1)/2;
    for(uint8_t n=0;n<half;n++) {
      int y1=7-n, y2=8+n;
      CRGB c=audioRainbow((uint8_t)(x*15+n*8));
      audioPixel(x,y1,c); audioPixel(x,y2,c);
    }
  }
}

// FFT 1 - same geometry as FFT0, but COLOR BELONGS TO THE ROW.
// Two red centre rows, then yellow, green, cyan, blue, violet outwards.
static CRGB fftRowColor(uint8_t distanceFromCenter) {
  static const CRGB rowPalette[8]={
    CRGB(255,20,20),   // centre pair
    CRGB(255,185,0),   // yellow/orange
    CRGB(235,255,0),   // yellow-green
    CRGB(30,255,40),   // green
    CRGB(0,245,220),   // cyan
    CRGB(0,120,255),   // blue
    CRGB(120,40,255),  // violet
    CRGB(255,30,210)   // fuchsia
  };
  return rowPalette[min((uint8_t)7,distanceFromCenter)];
}

static void renderAudioFFT1(const uint8_t *b) {
  clearFramebuffer();
  for(uint8_t x=0;x<16;x++) {
    uint8_t v=bandHeight(b[x<8?x:15-x]);
    uint8_t half=(uint8_t)max(1,(int)(v+1)/2);
    for(uint8_t n=0;n<half && n<8;n++) {
      CRGB c=fftRowColor(n);
      audioPixel(x,7-n,c);
      audioPixel(x,8+n,c);
    }
  }
}

// FFT 4 - validated: top/bottom bars moving toward centre.
static void renderAudioFFT3(const uint8_t *b) {
  clearFramebuffer();
  for(uint8_t x=0;x<16;x++) {
    uint8_t v=bandHeight(b[x<8?x:15-x]);
    uint8_t h=(uint8_t)min(7,(int)(v+1)/2);
    for(uint8_t n=0;n<h;n++) {
      CRGB c=audioRainbow((uint8_t)(150+x*11+n*7));
      audioPixel(x,n,c);
      audioPixel(x,15-n,c);
    }
  }
}

static uint8_t audioProtocolIndex(bool fft, uint8_t mode) {
  if (fft) return (uint8_t)(5 + mode);
  return mode;
}

// LEVEL 3 - dotted cyan frame + taller, colourful pseudo-spectrum.
static void renderAudioFakeSpectrum(uint8_t level) {
  clearFramebuffer();
  const CRGB frame(0,235,255);

  // Dotted border, not a continuous rectangle.
  for(int x=1;x<15;x+=2){ audioPixel(x,0,frame); audioPixel(x,15,frame); }
  for(int y=1;y<15;y+=2){ audioPixel(0,y,frame); audioPixel(15,y,frame); }

  uint8_t base=constrain((int)level,1,7);
  uint32_t t=millis()/95;
  for(int x=2;x<=13;x++) {
    // Central bars are intentionally taller; the original firmware synthesises
    // a spectrum from the single LEVEL value.
    int centreBoost=6-abs(x-7);
    uint8_t wobble=(uint8_t)((x*7+t*3+(x&1)*5)%5);
    uint8_t h=constrain((int)base+centreBoost/2+(int)wobble-1,2,13);
    for(uint8_t n=0;n<h;n++) {
      uint8_t hue=(uint8_t)(185 + x*10 + n*7 + t*2);
      audioPixel(x,14-n,audioRainbow(hue));
    }
  }
}

// FFT 2 - large rainbow heart at rest.  The heart is made of horizontal
// spectrum-like bars: music squeezes/expands each row instead of placing a
// separate equaliser underneath it.
static void renderAudioRainbowHeartFFT(const uint8_t *b) {
  clearFramebuffer();
  uint16_t sum=0;
  for(uint8_t i=0;i<8;i++) sum+=bandHeight(b[i]);
  uint8_t avg=(uint8_t)(sum/8);

  // Full-screen 14x13 heart silhouette expressed as left/right extents.
  static const int8_t left[13] ={3,1,0,0,0,1,1,2,3,4,5,6,7};
  static const int8_t right[13]={6,7,7,7,7,7,6,6,5,4,3,2,1};

  uint32_t phase=millis()/45;
  for(uint8_t y=0;y<13;y++) {
    int cx=7;
    int l=left[y], r=right[y];
    int baseHalf=max(l,r);

    // Each heart row follows one FFT band. Quiet -> full heart; louder values
    // produce visible in/out breathing while keeping the heart recognisable.
    uint8_t band=b[min((uint8_t)7,(uint8_t)(y/2))];
    int squeeze=(band>=8)?2:((band>=4)?1:0);
    if(avg<=2) squeeze=0;
    int half=max(1,baseHalf-squeeze);

    // Top lobes have two centres; lower rows one central bar.
    if(y<=4) {
      int lc=4, rc=11;
      int hw=max(1,3-squeeze/2);
      for(int x=lc-hw;x<=lc+hw;x++) audioPixel(x,y+1,audioRainbow((uint8_t)(x*13+y*12+phase)));
      for(int x=rc-hw;x<=rc+hw;x++) audioPixel(x,y+1,audioRainbow((uint8_t)(x*13+y*12+phase)));
      if(y>=2) for(int x=5+squeeze;x<=10-squeeze;x++) audioPixel(x,y+1,audioRainbow((uint8_t)(x*13+y*12+phase)));
    } else {
      for(int x=cx-half;x<=cx+half+1;x++) audioPixel(x,y+1,audioRainbow((uint8_t)(x*13+y*12+phase)));
    }
  }
}

// FFT 3 - validated horizontal spectrum growing from the vertical centre.
static void renderAudioHorizontalFromCenter(const uint8_t *b) {
  clearFramebuffer();
  for(int y=0;y<16;y++) { audioPixel(7,y,CRGB(40,140,255)); audioPixel(8,y,CRGB(40,140,255)); }
  for(uint8_t y=0;y<16;y++) {
    uint8_t src=(y<8)?y:(15-y);
    uint8_t v=bandHeight(b[src]);
    uint8_t w=(uint8_t)min(7,(int)((v+1)/2));
    for(uint8_t n=1;n<=w;n++) {
      CRGB c=audioRainbow((uint8_t)(y*14+n*9));
      audioPixel(7-n,y,c); audioPixel(8+n,y,c);
    }
  }
}

void renderAudio() {
  if(!audioState.valid) return;

  if (!audioState.fft) {
    switch(audioState.mode) {
      case 0: renderAudioBreakdancer(audioState.level); break;  // LEVEL 1
      case 1: renderAudioHeartLevel(audioState.level); break;   // LEVEL 2
      case 2: renderAudioFakeSpectrum(audioState.level); break; // LEVEL 3
      case 3: renderAudioPurpleFace(audioState.level); break; // LEVEL 4
      default: renderAudioRobotFace(audioState.level); break;   // LEVEL 5
    }
  } else {
    switch(audioState.mode) {
      case 0: renderAudioFFT0(audioState.bands); break;              // FFT 1 validated
      case 1: renderAudioFFT1(audioState.bands); break;              // FFT 2 row-coloured
      case 2: renderAudioRainbowHeartFFT(audioState.bands); break;   // FFT 3 full-screen heart
      case 3: renderAudioHorizontalFromCenter(audioState.bands); break; // FFT 4 validated
      default: renderAudioFFT3(audioState.bands); break;             // FFT 5 validated
    }
  }
  scaleLegacy16CanvasToLogical();
  refreshMatrix();
}

bool processAudioPacket(const uint8_t *data, size_t len) {
  // LEVEL family: MODE is 1..5 (last byte), LEVEL is byte 4.
  if (len == 6 && data[0] == 0x06 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x02) {
    uint8_t rawMode=data[5];
    if(rawMode<1 || rawMode>5) return false;
    audioState.valid=true;
    audioState.fft=false;
    audioState.mode=rawMode-1;
    audioState.level=(uint8_t)min(12,(int)data[4]);
    audioState.lastPacketMs=millis();
    audioState.packetCounter++;
    switchDisplayMode(DISPLAY_AUDIO);
    renderAudio();
#if DEBUG_SERIAL
    int uiEffect=audioProtocolIndex(false, audioState.mode);
    if(uiEffect!=audioLastUiEffect || millis()-audioLastLogMs>=1000) {
      Serial.print("AUDIO FX="); Serial.print(uiEffect);
      Serial.print(" LEVEL mode="); Serial.print(rawMode);
      Serial.print(" level="); Serial.println(audioState.level);
      audioLastUiEffect=uiEffect; audioLastLogMs=millis();
    }
#endif
    sendCommandAck(0x00,0x02);
    return true;
  }

  // FFT family. A complete logical frame is 21 bytes. In the captures a
  // 33-byte BLE write contains one full 21-byte frame plus the first 12
  // bytes of the following frame, therefore only bytes 0..20 are consumed.
  if (len >= 21 && data[0] == 0x21 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x02) {
    uint8_t mode=data[4];
    if(mode>4) return false;
    audioState.valid=true;
    audioState.fft=true;
    audioState.mode=mode;
    for(uint8_t i=0;i<8;i++) audioState.bands[i]=(uint8_t)min(12,(int)data[5+i]);
    audioState.lastPacketMs=millis();
    audioState.packetCounter++;
    switchDisplayMode(DISPLAY_AUDIO);
    renderAudio();
#if DEBUG_SERIAL
    int uiEffect=audioProtocolIndex(true, mode);
    if(uiEffect!=audioLastUiEffect || millis()-audioLastLogMs>=1000) {
      Serial.print("AUDIO FX="); Serial.print(uiEffect);
      Serial.print(" FFT mode="); Serial.print(mode);
      Serial.print(" bands=");
      for(uint8_t i=0;i<8;i++){ if(i)Serial.print(','); Serial.print(audioState.bands[i]); }
      Serial.println();
      audioLastUiEffect=uiEffect; audioLastLogMs=millis();
    }
#endif
    sendCommandAck(0x01,0x02);
    return true;
  }
  return false;
}

bool handleScheduleCommand(const uint8_t *data, size_t len);

// ======================================================
// COMMAND PROCESSOR
// ======================================================
void processFA02Packet(const uint8_t *data,size_t len){
  if(len<2) return;
  if(processAudioPacket(data,len)) return;
  if(processAlarmCommand(data,len)) return;
  if(processBulkPacket(data,len)) return;
  if(len<4) return;
  if(processEffectCommand(data,len)) return;
  uint8_t cmd=data[2], sub=data[3];

  if(len==4 && cmd==0x01 && sub==0x80){ sendDeviceInfo(); return; }

  if(len==11 && cmd==0x01 && sub==0x80){
    uint16_t y=2000+data[4];
    uint8_t mo=data[5], d=data[6], h=data[8], mi=data[9], se=data[10];
    if(isValidDateTime(y,mo,d,h,mi,se)){
      syncYear=y; syncMonth=mo; syncDay=d; syncHour=h; syncMinute=mi; syncSecond=se; syncMillis=millis(); clockSynced=true;
#if RTC_ENABLED && RTC_SYNC_FROM_BLE
      if(rtcReady){
        rtc.adjust(DateTime(syncYear,syncMonth,syncDay,syncHour,syncMinute,syncSecond));
        rtcTimeValid=true;
#if DEBUG_SERIAL
        Serial.println("RTC: synchronized from BLE time");
#endif
      }
#endif
    } else {
#if DEBUG_SERIAL
      Serial.print("TIME SYNC rejected: "); Serial.print(y); Serial.print('-'); Serial.print(mo); Serial.print('-'); Serial.print(d);
      Serial.print(' '); Serial.print(h); Serial.print(':'); Serial.print(mi); Serial.print(':'); Serial.println(se);
#endif
    }
    // ACK compatibility is preserved even when malformed fields are ignored;
    // negative/error ACK semantics are not yet experimentally established.
    sendCommandAck(0x01,0x80); return;
  }

  if(len==5 && cmd==0x07 && sub==0x01){
    const bool wasOn=screenOn;
    screenOn=data[4]!=0;
#if APP_POWER_VERSION_SPLASH
    if(screenOn && !wasOn){
      versionSplashActive=true;
      versionSplashUntil=millis()+VERSION_SPLASH_MS;
#if DEBUG_SERIAL
      Serial.print("VERSION SPLASH R"); Serial.print(FW_RELEASE_MAJOR); Serial.print('.'); Serial.print(FW_RELEASE_MINOR);
      Serial.print(" B"); Serial.println(FW_BUILD);
#endif
    } else if(!screenOn){
      versionSplashActive=false;
    }
#endif
    setStatusLed(screenOn); refreshMatrix(); sendCommandAck(cmd,sub); return;
  }
  if(len==5 && cmd==0x06 && sub==0x80){ flipped180=data[4]!=0; refreshMatrix(); sendCommandAck(cmd,sub); return; }

  if(len==10 && cmd==0x02 && sub==0x80){
    bool valid=data[5]<24 && data[6]<60 && data[7]<24 && data[8]<60 && data[9]<=100;
    if(valid){
      energySaving.enabled=data[4]!=0; energySaving.startHour=data[5]; energySaving.startMinute=data[6]; energySaving.endHour=data[7]; energySaving.endMinute=data[8]; energySaving.reductionPercent=data[9];
      refreshMatrix();
    } else {
#if DEBUG_SERIAL
      Serial.println("POWER SAVING rejected: invalid time/reduction fields");
#endif
    }
    // Preserve the compatibility ACK: an original-device error status has not
    // been established for malformed power-saving records.
    sendCommandAck(cmd,sub); return;
  }

  if(len==4 && cmd==0x03 && sub==0x80){ sendCommandAck(cmd,sub); pendingSoftReset=true; softResetAt=millis()+100; return; }

  if(len==8 && cmd==0x0A && sub==0x80){
    scoreA=(uint16_t)data[4]|((uint16_t)data[5]<<8); scoreB=(uint16_t)data[6]|((uint16_t)data[7]<<8);
    switchDisplayMode(DISPLAY_SCOREBOARD); renderScoreboard(); sendCommandAck(cmd,sub); return;
  }

  if(len==5 && cmd==0x04 && sub==0x01){
    bool enter=data[4]!=0;
    if(enter && !diyMode){ switchDisplayMode(DISPLAY_GRAFFITI); clearFramebuffer(); refreshMatrix(); }
    diyMode=enter; sendCommandAck(cmd,sub); return;
  }

  if(len==5 && cmd==0x04 && sub==0x80){
    brightnessPercent=min((uint8_t)100,data[4]);
    refreshMatrix();
    scheduleBrightnessSave();
#if DEBUG_SERIAL
    Serial.print("BRIGHTNESS RX: "); Serial.print(brightnessPercent); Serial.println("%");
#endif
    sendCommandAck(cmd,sub);
    return;
  }

  if(len==7 && cmd==0x02 && sub==0x02){ switchDisplayMode(DISPLAY_SOLID); clearFramebuffer(CRGB(data[4],data[5],data[6])); refreshMatrix(); sendCommandAck(cmd,sub); return; }

  if(len==8 && cmd==0x06 && sub==0x01){
    uint8_t flags=data[4]; clockStyle=flags&0x3F; clock24h=(flags&0x40)!=0; clockShowDate=(flags&0x80)!=0; clockColor=CRGB(data[5],data[6],data[7]);
#if DEBUG_SERIAL
    Serial.print("CLOCK RAW ["); Serial.print(len); Serial.print("]: "); dumpHex(data,len);
    Serial.print("CLOCK style raw="); Serial.print(clockStyle);
    Serial.print(" case="); Serial.print(clockStyle & 0x07);
    Serial.print(" 24H="); Serial.print(clock24h ? 1 : 0);
    Serial.print(" DATE="); Serial.print(clockShowDate ? 1 : 0);
    Serial.print(" RGB="); Serial.print(data[5]); Serial.print(','); Serial.print(data[6]); Serial.print(','); Serial.println(data[7]);
#endif
    clockCycleStartedAt=millis();
    switchDisplayMode(DISPLAY_CLOCK); renderClock(); sendCommandAck(cmd,sub); return;
  }

  if(len==7 && cmd==0x08 && sub==0x80){
    uint8_t mode=data[4]; uint32_t req=((uint32_t)data[5]*60UL+data[6])*1000UL;
    switch(mode){
      case 0:cancelCountdownFinishBuzzer();countdownRunning=false;countdownPaused=false;countdownRemainingMs=0;countdownFinishSent=false;break;
      case 1:cancelCountdownFinishBuzzer();countdownRemainingMs=req;countdownStartMillis=millis();countdownRunning=true;countdownPaused=false;countdownFinishSent=false;break;
      case 2:if(countdownRunning){uint32_t e=millis()-countdownStartMillis;countdownRemainingMs=(e<countdownRemainingMs)?countdownRemainingMs-e:0;}countdownRunning=false;countdownPaused=true;break;
      case 3:if(countdownRemainingMs){countdownStartMillis=millis();countdownRunning=true;countdownPaused=false;}break;
    }
    switchDisplayMode(DISPLAY_COUNTDOWN); sendCommandAck(cmd,sub); return;
  }

  if(len==5 && cmd==0x09 && sub==0x80){
    uint8_t mode=data[4];
    switch(mode){
      case 0:stopwatchRunning=false;stopwatchElapsedMs=0;break;
      case 1:stopwatchElapsedMs=0;stopwatchStartMillis=millis();stopwatchRunning=true;break;
      case 2:if(stopwatchRunning)stopwatchElapsedMs+=millis()-stopwatchStartMillis;stopwatchRunning=false;break;
      case 3:if(!stopwatchRunning){stopwatchStartMillis=millis();stopwatchRunning=true;}break;
    }
    switchDisplayMode(DISPLAY_STOPWATCH); sendCommandAck(cmd,sub); return;
  }

  if(len>=10 && cmd==0x05 && sub==0x01){
    switchDisplayMode(DISPLAY_GRAFFITI);
    uint8_t r=data[5],g=data[6],b=data[7];
    for(size_t i=8;i+1<len;i+=2){ uint8_t x=data[i],y=data[i+1]; if(x<MATRIX_WIDTH&&y<MATRIX_HEIGHT) framebuffer[logicalIndex(x,y)]=CRGB(r,g,b); }
    refreshMatrix(); return;
  }

  if (handleCarouselCommand(data, len)) return;
  if (handleScheduleCommand(data, len)) return;
  if (handlePresetCommand(data, len)) return;

  // Unknown command: always latch it for the OLED, even when Serial debug is off.
  unknownCommandActive = true;
  unknownCommandAt = millis();
  unknownCommandCount++;
  unknownCommandLen = (uint16_t)min(len, (size_t)0xFFFF);
  unknownCommandStored = (uint8_t)min(len, (size_t)OLED_UNKNOWN_BYTES);
  for (uint8_t i=0; i<unknownCommandStored; i++) unknownCommandData[i]=data[i];
#if DEBUG_SERIAL
  Serial.print("UNHANDLED COMMAND [");Serial.print(len);Serial.print("]: ");dumpHex(data,len);
#endif
  sendCommandAck(cmd,sub);
}

// ======================================================
// BLE CALLBACKS
// ======================================================
// Caller must hold runtimeStateMutex: this function mutates reassembly, Bulk and renderer state.
void processFA02Write(const uint8_t *data, size_t len) {
  if(!data || !len) return;
  const uint32_t now=millis();
  expireStalledTransfers(now);
  if(bulk.active) bulkLastRxMs=now;

  // A single ATT write can complete one logical packet and already contain
  // bytes from the next one. Consume the write in bounded slices instead of
  // discarding trailing bytes. Also preserve a split two-byte length header.
  size_t offset=0;
  while(offset<len){
    if(packetReceived<2){
      const size_t needHeader=2U-packetReceived;
      const size_t available=len-offset;
      const size_t take=needHeader<available?needHeader:available;
      ::memcpy(packetBuffer+packetReceived,data+offset,take);
      packetReceived+=take;
      offset+=take;
      packetLastRxMs=now;
      if(packetReceived<2) return;

      packetExpected=(uint16_t)packetBuffer[0]|((uint16_t)packetBuffer[1]<<8);
      if(packetExpected<2||packetExpected>MAX_PACKET_SIZE){
#if DEBUG_SERIAL
        Serial.print("PACKET RX invalid declared length="); Serial.println(packetExpected);
#endif
        packetExpected=packetReceived=0; packetLastRxMs=0; resetBulkTransfer(true); return;
      }
    }

    const size_t needed=(size_t)packetExpected-packetReceived;
    const size_t available=len-offset;
    const size_t take=needed<available?needed:available;
    if(packetReceived+take>MAX_PACKET_SIZE){
      packetExpected=packetReceived=0; packetLastRxMs=0; resetBulkTransfer(true); return;
    }
    if(take){
      ::memcpy(packetBuffer+packetReceived,data+offset,take);
      packetReceived+=take;
      offset+=take;
      packetLastRxMs=now;
    }

#if DEBUG_SERIAL && DEBUG_SERIAL_VERBOSE
    if(bulk.active || packetExpected>=512){
      Serial.print("[RXASM] write="); Serial.print(len);
      Serial.print(" consumed="); Serial.print(offset);
      Serial.print(" packet="); Serial.print(packetReceived);
      Serial.print('/'); Serial.println(packetExpected);
    }
#endif

    if(packetReceived<packetExpected) return;

    processFA02Packet(packetBuffer,packetExpected);
    packetReceived=packetExpected=0;
    packetLastRxMs=0;
    // Continue if this ATT write already contains the next logical packet.
  }
}

class FA02Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    String value=c->getValue(); if(!value.length()) return;
    if(!lockRuntimeState()) return;
    processFA02Write((const uint8_t*)value.c_str(),value.length());
    unlockRuntimeState();
  }
};

class AE01Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
#if DEBUG_SERIAL && DEBUG_SERIAL_VERBOSE
    String v=c->getValue(); if(!v.length()) return; Serial.print("RX AE01 [");Serial.print(v.length());Serial.print("]: ");dumpHex((const uint8_t*)v.c_str(),v.length());
#endif
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
#if PNG_DIAG_SERIAL
    Serial.println("BLE C");
#endif
    if(!lockRuntimeState()) return;
    deviceConnected=true;
    // BLE connection must not change display power/state. The boot policy or
    // the latest app command remains authoritative, avoiding ON+black state.
    triggerConnectionBuzzer();
    pendingAdvertisingRestart=false;
    pendingDeviceInfoPush=true;
    deviceInfoPushAt=millis()+1200;
    unlockRuntimeState();
  }
  void onDisconnect(BLEServer*) override {
#if PNG_DIAG_SERIAL
    Serial.println("BLE D");
#endif
    if(!lockRuntimeState()) return;
    deviceConnected=false;
    packetReceived=packetExpected=0;
    packetLastRxMs=0;
    resetBulkTransfer(true);
    // Do not sleep inside the BLE callback. Preserve the historical 300 ms
    // restart delay by deferring advertising restart to the Arduino loop.
    pendingAdvertisingRestart=true;
    advertisingRestartAt=millis()+300;
    unlockRuntimeState();
  }
};

// ======================================================
// OTA
// ======================================================

// ======================================================
// RESET RUNTIME
// ======================================================
void resetRuntimeState(){
  // An app-issued reset clears emulator-managed persistent device
  // state but is not treated as an electrical power cycle. BLE remains up,
  // the already synchronized software clock is preserved, and the logical
  // screen remains ON with a black framebuffer ready for the next command.
  int8_t queuedGifSlot=pendingGifSlot;
  carouselEnterRequested=false; carouselUploadOpen=false; carouselUploadBlackout=false; carouselStartPending=false;
  carouselActive=false; carouselActiveSlot=-1;
  freeGIF();
  if(littleFsReady && queuedGifSlot>=0 && queuedGifSlot<=1) LittleFS.remove(GIF_RX_FILES[(uint8_t)queuedGifSlot]);
  alarmActive=false; activeAlarmSlot=0xFF; alarmEndsAt=0;
  scheduleActiveIndex=-1; scheduleFailedIndex=-1;
  diyMode=false; effectState.valid=false; textState.valid=false; audioState.valid=false;
  cancelCountdownFinishBuzzer(); cancelScheduleStartBuzzer(); cancelConnectionBuzzer(); countdownRunning=false; countdownPaused=false; countdownRemainingMs=0; countdownFinishSent=false;
  stopwatchRunning=false; stopwatchElapsedMs=0; scoreA=scoreB=0;
  displayMode=DISPLAY_NONE;
  clearFramebuffer();
  clearPersistentDeviceState();
  screenOn=true;
  setStatusLed(true);
  clearFramebuffer();
  refreshMatrix();
#if DEBUG_SERIAL
  Serial.println("DEVICE RESET: runtime ready, screen ON/black, volatile time preserved");
#endif
}

// ======================================================
// SETUP
// ======================================================


// -----------------------------------------------------------------------------
// PROGRAM / SCHEDULE
// The app stores the Programs; the device stores only the active Schedule.
// 07 80 -> global state (bit0 enabled, bit1 sound), ACK=01
// 05 80 -> activity media chunk: ACK=01 while incomplete, ACK=03 on final CRC-valid chunk
// A new list is received in staging and committed after a short timeout.
// -----------------------------------------------------------------------------
struct ScheduleActivity {
  bool configured = false;
  uint8_t flags = 0;            // bit0 enabled, bit1..7 lun..dom
  uint8_t startHour = 0;
  uint8_t startMinute = 0;
  uint8_t endHour = 0;
  uint8_t endMinute = 0;
  uint16_t contentType = 0;
  uint32_t mediaSize = 0;
  uint32_t mediaCRC = 0;
  uint16_t reserved = 0;
  uint8_t mediaId = 0;
};

ScheduleActivity scheduleActivities[SCHEDULE_MAX_ACTIVITIES];
ScheduleActivity scheduleStaging[SCHEDULE_MAX_ACTIVITIES];
Preferences schedulePrefs;
uint8_t scheduleGlobalFlags = 0;
uint8_t scheduleStagingGlobalFlags = 0;
bool scheduleUploadOpen = false;
bool scheduleUploadDirty = false;
uint32_t scheduleLastRxMs = 0;
uint32_t scheduleReceivedMask = 0;
int8_t scheduleActiveIndex = -1;
int8_t scheduleFailedIndex = -1;  // evita retry continuo se un media non viene decodificato
DisplayMode schedulePreviousMode = DISPLAY_CLOCK;
bool schedulePreviousCarousel = false;
int8_t schedulePreviousCarouselSlot = -1;
CRGB *scheduleSavedFrame = nullptr;

// BUILD 136: hardware logs decoded the byte that appeared to change the
// Schedule media type from 1 to 513. The field at offset 10 is the one-byte
// content type. Offset 11 is a per-chunk marker: observed 0x00 on the first
// chunk and 0x02 on continuation chunks. It is framing metadata, not part of
// contentType and therefore must never reset staging. The complete media object
// remains identified by contentType + mediaSize + mediaCRC.
struct ScheduleMediaUploadState {
  bool active = false;
  ScheduleActivity candidate;
  uint32_t received = 0;
  uint32_t runningCRC = 0xFFFFFFFFUL;
  uint32_t lastChunkAt = 0;
};

ScheduleMediaUploadState scheduleMediaUploads[SCHEDULE_MAX_ACTIVITIES];

bool scheduleMediaObjectMatches(const ScheduleActivity &a, const ScheduleActivity &b) {
  // For a multi-packet activity the media object itself is identified by type,
  // declared complete size and complete-object CRC. BUILD 134 proved that one
  // or more ancillary header fields can change between chunks, so they must
  // not be used to decide whether the payload is a continuation.
  return a.contentType == b.contentType &&
         a.mediaSize == b.mediaSize &&
         a.mediaCRC == b.mediaCRC;
}

bool scheduleActivityHeaderMetadataMatches(const ScheduleActivity &a, const ScheduleActivity &b) {
  return a.flags == b.flags &&
         a.startHour == b.startHour && a.startMinute == b.startMinute &&
         a.endHour == b.endHour && a.endMinute == b.endMinute &&
         a.reserved == b.reserved &&
         a.mediaId == b.mediaId;
}

static inline uint16_t rd16le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t rd32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

String scheduleFileName(uint8_t idx) { return String("/sch") + idx + ".bin"; }
String scheduleTempFileName(uint8_t idx) { return String("/scht") + idx + ".bin"; }
String scheduleBackupFileName(uint8_t idx) { return String("/schb") + idx + ".bin"; }

bool saveScheduleGlobalValue(uint8_t flags) {
  return schedulePrefs.putUChar("flags", flags) == 1;
}

bool saveScheduleGlobal() {
  return saveScheduleGlobalValue(scheduleGlobalFlags);
}

bool saveScheduleMetaValue(uint8_t idx, const ScheduleActivity &value) {
  if (idx >= SCHEDULE_MAX_ACTIVITIES) return false;
  char key[10]; snprintf(key, sizeof(key), "s%u", idx);
  return schedulePrefs.putBytes(key, &value, sizeof(ScheduleActivity)) == sizeof(ScheduleActivity);
}

bool saveScheduleMeta(uint8_t idx) {
  return idx < SCHEDULE_MAX_ACTIVITIES && saveScheduleMetaValue(idx, scheduleActivities[idx]);
}

bool removeScheduleMetaValue(uint8_t idx) {
  if (idx >= SCHEDULE_MAX_ACTIVITIES) return false;
  char key[10]; snprintf(key, sizeof(key), "s%u", idx);
  if (!schedulePrefs.isKey(key)) return true;
  return schedulePrefs.remove(key);
}

void cleanupScheduleTempFiles() {
  if(!littleFsReady) return;
  for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) LittleFS.remove(scheduleTempFileName(i));
}

void cleanupScheduleBackupFiles() {
  if(!littleFsReady) return;
  for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) LittleFS.remove(scheduleBackupFileName(i));
}

void cleanupScheduleTransactionFiles() {
  cleanupScheduleTempFiles();
  cleanupScheduleBackupFiles();
}

void recoverScheduleFile(uint8_t idx) {
  if (!littleFsReady || idx >= SCHEDULE_MAX_ACTIVITIES) return;
  String dst=scheduleFileName(idx), tmp=scheduleTempFileName(idx), bak=scheduleBackupFileName(idx);
  LittleFS.remove(tmp);

  const ScheduleActivity &a=scheduleActivities[idx];
  if (!a.configured || !a.mediaSize) {
    LittleFS.remove(dst);
    LittleFS.remove(bak);
    return;
  }

  bool dstOK=fileMatchesMedia(dst,a.mediaSize,a.mediaCRC);
  bool bakOK=fileMatchesMedia(bak,a.mediaSize,a.mediaCRC);
  if(dstOK){
    LittleFS.remove(bak);
  } else if(bakOK){
    LittleFS.remove(dst);
    if(!LittleFS.rename(bak,dst)){
#if DEBUG_SERIAL
      Serial.print("SCH RECOVERY rename failed i="); Serial.println(idx);
#endif
    }
  }
}

void loadSchedule() {
  schedulePrefs.begin("idot-sched", false);
  scheduleGlobalFlags = schedulePrefs.getUChar("flags", 0);
  scheduleStagingGlobalFlags = scheduleGlobalFlags;
  uint8_t count = 0;
  for (uint8_t i = 0; i < SCHEDULE_MAX_ACTIVITIES; i++) {
    char key[10]; snprintf(key, sizeof(key), "s%u", i);
    size_t n = schedulePrefs.getBytesLength(key);
    if (n == sizeof(ScheduleActivity)) {
      schedulePrefs.getBytes(key, &scheduleActivities[i], sizeof(ScheduleActivity));
      if (scheduleActivities[i].configured) count++;
    }
    recoverScheduleFile(i);
  }

  Serial.print("SCH LOAD "); Serial.print(scheduleGlobalFlags, HEX); Serial.print("/"); Serial.println(count);
}

bool scheduleHasActiveMediaUpload() {
  for (uint8_t i=0; i<SCHEDULE_MAX_ACTIVITIES; ++i)
    if (scheduleMediaUploads[i].active) return true;
  return false;
}

void clearScheduleMediaUploadStates() {
  for (uint8_t i=0; i<SCHEDULE_MAX_ACTIVITIES; ++i)
    scheduleMediaUploads[i] = ScheduleMediaUploadState();
}

void resetScheduleMediaUpload(uint8_t idx, bool removeTemp) {
  if (idx >= SCHEDULE_MAX_ACTIVITIES) return;
  if (removeTemp && littleFsReady) LittleFS.remove(scheduleTempFileName(idx));
  scheduleMediaUploads[idx] = ScheduleMediaUploadState();
}

void abortScheduleUploadTransaction(const char *reason) {
#if DEBUG_SERIAL
  if (reason) {
    Serial.print("SCH UPLOAD ABORT reason="); Serial.println(reason);
  }
#endif
  if (littleFsReady) cleanupScheduleTempFiles();
  clearScheduleMediaUploadStates();
  for (uint8_t i=0; i<SCHEDULE_MAX_ACTIVITIES; ++i)
    scheduleStaging[i] = ScheduleActivity();
  scheduleReceivedMask = 0;
  scheduleUploadOpen = false;
  scheduleUploadDirty = false;
}

bool beginScheduleMediaUpload(uint8_t idx, const ScheduleActivity &candidate) {
  if (!littleFsReady || idx >= SCHEDULE_MAX_ACTIVITIES || !candidate.mediaSize) return false;
  resetScheduleMediaUpload(idx, true);
  File f = LittleFS.open(scheduleTempFileName(idx), "w");
  if (!f) return false;
  f.close();
  ScheduleMediaUploadState &u = scheduleMediaUploads[idx];
  u.active = true;
  u.candidate = candidate;
  u.received = 0;
  u.runningCRC = 0xFFFFFFFFUL;
  u.lastChunkAt = millis();
  return true;
}

void updateScheduleMediaUploadTimeout() {
  uint32_t now = millis();
  for (uint8_t i=0; i<SCHEDULE_MAX_ACTIVITIES; ++i) {
    const ScheduleMediaUploadState &u = scheduleMediaUploads[i];
    if (u.active && (uint32_t)(now-u.lastChunkAt) > SCHEDULE_MEDIA_UPLOAD_TIMEOUT_MS) {
      abortScheduleUploadTransaction("media timeout");
      return;
    }
  }
}

void beginScheduleUpload(uint8_t flags) {
  scheduleStagingGlobalFlags = flags;
  scheduleUploadOpen = true;
  scheduleUploadDirty = false;
  scheduleFailedIndex = -1;
  scheduleReceivedMask = 0;
  scheduleLastRxMs = millis();
  clearScheduleMediaUploadStates();
  for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) {
    recoverScheduleFile(i);
    scheduleStaging[i] = ScheduleActivity();
  }

  Serial.print("SCH OPEN "); Serial.println(flags, HEX);
}

bool rollbackScheduleFilesystem(uint32_t backedUpMask, uint32_t promotedMask) {
  bool ok=true;
  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++){
    uint32_t bit=(1UL<<i);
    if(promotedMask & bit) LittleFS.remove(scheduleFileName(i));
  }
  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++){
    uint32_t bit=(1UL<<i);
    if(!(backedUpMask & bit)) continue;
    String bak=scheduleBackupFileName(i), dst=scheduleFileName(i);
    LittleFS.remove(dst);
    if(LittleFS.exists(bak) && !LittleFS.rename(bak,dst)) ok=false;
  }
  return ok;
}

void restoreSchedulePreferences(uint8_t oldFlags) {
  saveScheduleGlobalValue(oldFlags);
  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++){
    if(scheduleActivities[i].configured) saveScheduleMetaValue(i,scheduleActivities[i]);
    else removeScheduleMetaValue(i);
  }
}

bool commitScheduleUpload() {
  if (!littleFsReady) {
    scheduleUploadOpen=false; scheduleUploadDirty=false;
    return false;
  }
  if (!scheduleUploadOpen || !scheduleUploadDirty) {
    scheduleUploadOpen = false;
    return false;
  }
  // Never publish a partial list while one activity media object is still
  // being assembled from multiple logical packets.
  if (scheduleHasActiveMediaUpload()) return false;

  // Preflight every staged media file before touching the active Schedule.
  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++){
    if(!(scheduleReceivedMask & (1UL<<i))) continue;
    const ScheduleActivity &a=scheduleStaging[i];
    if(!a.configured || !fileMatchesMedia(scheduleTempFileName(i),a.mediaSize,a.mediaCRC)){
#if DEBUG_SERIAL
      Serial.print("SCH COMMIT preflight failed i="); Serial.println(i);
#endif
      scheduleUploadOpen=false; scheduleUploadDirty=false;
      cleanupScheduleTransactionFiles();
      clearScheduleMediaUploadStates();
      return false;
    }
  }

  uint32_t backedUpMask=0, promotedMask=0;
  bool fsOK=true;

  // Preserve every old destination first. Entries omitted by the new upload
  // remain only in backup until the new metadata has committed successfully.
  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES && fsOK;i++){
    String dst=scheduleFileName(i), bak=scheduleBackupFileName(i);
    LittleFS.remove(bak);
    if(LittleFS.exists(dst)){
      if(LittleFS.rename(dst,bak)) backedUpMask|=(1UL<<i);
      else fsOK=false;
    }
  }

  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES && fsOK;i++){
    uint32_t bit=(1UL<<i);
    if(!(scheduleReceivedMask & bit)) continue;
    if(LittleFS.rename(scheduleTempFileName(i),scheduleFileName(i))) promotedMask|=bit;
    else fsOK=false;
  }

  if(!fsOK){
    bool rollbackOK=rollbackScheduleFilesystem(backedUpMask,promotedMask);
    scheduleUploadOpen=false; scheduleUploadDirty=false;
    cleanupScheduleTempFiles();
    clearScheduleMediaUploadStates();
    if(rollbackOK) cleanupScheduleBackupFiles();
#if DEBUG_SERIAL
    Serial.println("SCH COMMIT filesystem rollback");
#endif
    return false;
  }

  // NVS is updated only after all media files are in place. Runtime state is
  // published last; on a preference failure the previous files/metadata are
  // restored best-effort and the active Schedule remains unchanged.
  uint8_t oldFlags=scheduleGlobalFlags;
  bool prefsOK=saveScheduleGlobalValue(scheduleStagingGlobalFlags);
  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES && prefsOK;i++){
    if(scheduleReceivedMask & (1UL<<i)) prefsOK=saveScheduleMetaValue(i,scheduleStaging[i]);
    else prefsOK=removeScheduleMetaValue(i);
  }

  if(!prefsOK){
    restoreSchedulePreferences(oldFlags);
    bool rollbackOK=rollbackScheduleFilesystem(backedUpMask,promotedMask);
    scheduleUploadOpen=false; scheduleUploadDirty=false;
    cleanupScheduleTempFiles();
    clearScheduleMediaUploadStates();
    if(rollbackOK) cleanupScheduleBackupFiles();
#if DEBUG_SERIAL
    Serial.println("SCH COMMIT preference rollback");
#endif
    return false;
  }

  if(scheduleActiveIndex>=0) stopScheduleActivity();
  scheduleGlobalFlags=scheduleStagingGlobalFlags;
  for(uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++){
    if(scheduleReceivedMask & (1UL<<i)) scheduleActivities[i]=scheduleStaging[i];
    else scheduleActivities[i]=ScheduleActivity();
    LittleFS.remove(scheduleBackupFileName(i));
    LittleFS.remove(scheduleTempFileName(i));
  }
  scheduleUploadOpen=false;
  scheduleUploadDirty=false;
  scheduleFailedIndex=-1;
  clearScheduleMediaUploadStates();

  Serial.print("SCH COMMIT "); Serial.println(scheduleReceivedMask, HEX);
  return true;
}

bool scheduleTimeInside(const ScheduleActivity &a, uint16_t nowMin) {
  uint16_t s = (uint16_t)a.startHour * 60U + a.startMinute;
  uint16_t e = (uint16_t)a.endHour * 60U + a.endMinute;
  if (s == e) return true;                 // 24h
  if (s < e) return nowMin >= s && nowMin < e;
  return nowMin >= s || nowMin < e;        // attraversa mezzanotte
}

static inline uint32_t pngBE32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint8_t pngPaeth(uint8_t a, uint8_t b, uint8_t c) {
  int p = (int)a + (int)b - (int)c;
  int pa = abs(p - (int)a), pb = abs(p - (int)b), pc = abs(p - (int)c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

// Small PNG decoder for app-supplied Schedule images. The expected image
// dimensions follow the selected logical iDotMatrix profile. Supports 8-bit,
// non-interlaced RGB (type 2) and RGBA (type 6).
// La decompressione DEFLATE viene eseguita dal miniz dell'ESP32.
bool decodeSchedulePNG(File &f, uint32_t fileSize) {
#if PNG_DIAG_SERIAL
  Serial.print("P0 n="); Serial.print(fileSize);
  Serial.print(" h="); Serial.println(ESP.getFreeHeap());
#endif
  if (!f || fileSize < 33 || fileSize > 65536UL) { PDBGLN("PF0"); return false; }

  uint8_t *png = (uint8_t*)malloc(fileSize);
  if (!png) { PDBGLN("PF1"); return false; }
  size_t got = f.read(png, fileSize);
  if (got != fileSize) { free(png); PDBGLN("PF2"); return false; }

  static const uint8_t sig[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
  if (::memcmp(png, sig, 8) != 0) { free(png); PDBGLN("PF3"); return false; }

  uint32_t w=0, h=0, idatBytes=0;
  uint8_t depth=0, colorType=0, comp=0, filterMethod=0, interlace=0;
  size_t pos=8;
  while (pos + 12 <= fileSize) {
    uint32_t n = pngBE32(png + pos);
    if ((uint64_t)pos + 12ULL + n > fileSize) { free(png); PDBGLN("PF4"); return false; }
    const uint8_t *type = png + pos + 4;
    const uint8_t *data = png + pos + 8;
    if (!::memcmp(type, "IHDR", 4)) {
      if (n != 13) { free(png); PDBGLN("PF5"); return false; }
      w=pngBE32(data); h=pngBE32(data+4); depth=data[8]; colorType=data[9];
      comp=data[10]; filterMethod=data[11]; interlace=data[12];
    } else if (!::memcmp(type, "IDAT", 4)) idatBytes += n;
    else if (!::memcmp(type, "IEND", 4)) break;
    pos += 12 + n;
  }
#if PNG_DIAG_SERIAL
  Serial.print("P1 "); Serial.print(w); Serial.print('x'); Serial.print(h);
  Serial.print(" ct="); Serial.print(colorType); Serial.print(" z="); Serial.println(idatBytes);
#endif
  if (w != MATRIX_WIDTH || h != MATRIX_HEIGHT || depth != 8 || comp != 0 || filterMethod != 0 ||
      interlace != 0 || (colorType != 2 && colorType != 6) || !idatBytes) {
    free(png); PDBGLN("PF6"); return false;
  }

  uint8_t bpp = (colorType == 6) ? 4 : 3;
  const size_t rowBytes = (size_t)w * bpp;
  const size_t rawSize = (rowBytes + 1) * h;
  uint8_t *idat = (uint8_t*)malloc(idatBytes);
  if (!idat) { free(png); PDBGLN("PF7"); return false; }

  pos=8; size_t io=0;
  while (pos + 12 <= fileSize) {
    uint32_t n=pngBE32(png+pos); const uint8_t *type=png+pos+4;
    if ((uint64_t)pos + 12ULL + n > fileSize) break;
    if (!::memcmp(type,"IDAT",4)) { ::memcpy(idat+io,png+pos+8,n); io+=n; }
    if (!::memcmp(type,"IEND",4)) break;
    pos += 12 + n;
  }
  free(png);
  if (io != idatBytes) { free(idat); PDBGLN("PF8"); return false; }

  uint8_t *raw = (uint8_t*)malloc(rawSize);
  if (!raw) { free(idat); PDBGLN("PF9"); return false; }
#if PNG_DIAG_SERIAL
  Serial.print("P2 raw="); Serial.print(rawSize); Serial.print(" h="); Serial.println(ESP.getFreeHeap());
  Serial.println("P3 inflate");
  Serial.flush();
#endif
  // IMPORTANT: non usare tinfl_decompress_mem_to_mem() qui. Quel wrapper
  // crea un tinfl_decompressor locale molto grande sullo stack del loopTask
  // e sull'ESP32 provoca lo stack-canary visto nei log. Manteniamo invece
  // keep the inflater state on the heap and call the low-level API.
  tinfl_decompressor *infl = (tinfl_decompressor*)malloc(sizeof(tinfl_decompressor));
  if (!infl) { free(idat); free(raw); PDBGLN("PF10A"); return false; }
  tinfl_init(infl);
  size_t inBytes = idatBytes;
  size_t outBytes = rawSize;
  tinfl_status inflateStatus = tinfl_decompress(
      infl,
      idat, &inBytes,
      raw, raw, &outBytes,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  free(infl);
  free(idat);
#if PNG_DIAG_SERIAL
  Serial.print("P4 st="); Serial.print((int)inflateStatus);
  Serial.print(" in="); Serial.print(inBytes);
  Serial.print(" out="); Serial.print(outBytes);
  Serial.print(" h="); Serial.print(ESP.getFreeHeap());
  Serial.print(" stk="); Serial.println(uxTaskGetStackHighWaterMark(nullptr));
  Serial.flush();
#endif
  if (inflateStatus != TINFL_STATUS_DONE || outBytes != rawSize) {
    free(raw); PDBGLN("PF10"); return false;
  }

  for (uint8_t y=0; y<h; y++) {
    uint8_t *row = raw + (size_t)y * (rowBytes + 1);
    uint8_t ft = row[0];
    uint8_t *cur = row + 1;
    uint8_t *prev = y ? (raw + (size_t)(y-1) * (rowBytes + 1) + 1) : nullptr;
    if (ft > 4) { free(raw); PDBGLN("PF11"); return false; }
    for (size_t x=0; x<rowBytes; x++) {
      uint8_t left = (x >= bpp) ? cur[x-bpp] : 0;
      uint8_t up = prev ? prev[x] : 0;
      uint8_t ul = (prev && x >= bpp) ? prev[x-bpp] : 0;
      switch (ft) {
        case 0: break;
        case 1: cur[x] = (uint8_t)(cur[x] + left); break;
        case 2: cur[x] = (uint8_t)(cur[x] + up); break;
        case 3: cur[x] = (uint8_t)(cur[x] + ((uint16_t)left + up)/2U); break;
        case 4: cur[x] = (uint8_t)(cur[x] + pngPaeth(left,up,ul)); break;
      }
    }
  }
  PDBGLN("P5 filter");

  clearFramebuffer();
  for (uint8_t y=0; y<MATRIX_HEIGHT; y++) {
    const uint8_t *row = raw + (size_t)y * (rowBytes + 1) + 1;
    for (uint8_t x=0; x<MATRIX_WIDTH; x++) {
      const uint8_t *p = row + (size_t)x * bpp;
      uint8_t r=p[0], g=p[1], b=p[2];
      if (bpp == 4) {
        uint8_t a=p[3];
        r=(uint8_t)(((uint16_t)r*a + 127U)/255U);
        g=(uint8_t)(((uint16_t)g*a + 127U)/255U);
        b=(uint8_t)(((uint16_t)b*a + 127U)/255U);
      }
      framebuffer[logicalIndex(x,y)] = CRGB(r,g,b);
    }
  }
  free(raw);
  PDBGLN("P6 show");
  displayMode = DISPLAY_RAW;
  refreshMatrix();
  PDBGLN("P7 done");
  return true;
}

bool loadScheduleMedia(uint8_t idx) {
  if (!littleFsReady || idx >= SCHEDULE_MAX_ACTIVITIES) return false;
  ScheduleActivity &a = scheduleActivities[idx];
  if (!a.configured || !a.mediaSize) return false;
  File f = LittleFS.open(scheduleFileName(idx), "r");
  if (!f || (uint32_t)f.size() != a.mediaSize) { if(f)f.close(); return false; }

  if (a.contentType == SCHEDULE_CONTENT_GIF) {
    f.close();
    return startStoredGIFPlayback(scheduleFileName(idx), a.mediaSize, a.mediaCRC);
  }

  if (a.contentType == SCHEDULE_CONTENT_TEXT) {
    if (a.mediaSize > MAX_TEXT_PAYLOAD) { f.close(); return false; }
    size_t got = f.read(textPayload, a.mediaSize); f.close();
    if (got != a.mediaSize) return false;
    parseTextPayload(textPayload, got);
    return true;
  }

  if (a.contentType == SCHEDULE_CONTENT_IMAGE) {
    bool ok = decodeSchedulePNG(f, a.mediaSize);
    f.close();
  #if PNG_DIAG_SERIAL
    Serial.print("PR i="); Serial.print(idx); Serial.print(" ok="); Serial.println(ok ? 1 : 0);
#endif
    return ok;
  }

  f.close();
  return false;
}

void stopScheduleActivity() {
  if (scheduleActiveIndex < 0) return;
  scheduleActiveIndex = -1;
  stopGIFPlayback();
  if (schedulePreviousCarousel && nextCarouselSlot(-1) >= 0) {
    carouselEnterRequested = true;
    int8_t slot = schedulePreviousCarouselSlot;
    if (slot < 0 || slot >= CAROUSEL_SLOT_COUNT || !carouselSlots[(uint8_t)slot].configured)
      slot = nextCarouselSlot(-1);
    if (slot >= 0) startCarouselSlot((uint8_t)slot);
    schedulePreviousCarousel = false;
    schedulePreviousCarouselSlot = -1;
    return;
  }
  schedulePreviousCarousel = false;
  schedulePreviousCarouselSlot = -1;
  if (schedulePreviousMode == DISPLAY_SOLID || schedulePreviousMode == DISPLAY_RAW ||
      schedulePreviousMode == DISPLAY_GRAFFITI) {
    ::memcpy(framebuffer, scheduleSavedFrame, LOGICAL_FRAME_BYTES);
    displayMode = schedulePreviousMode;
    refreshMatrix();
  } else if (clockSynced) {
    displayMode = DISPLAY_CLOCK;
    renderClock();
  } else {
    displayMode = DISPLAY_NONE;
    clearFramebuffer(); refreshMatrix();
  }

#if PNG_DIAG_SERIAL
  Serial.println("S-");
#endif
}

void startScheduleActivity(uint8_t idx) {
  if (idx >= SCHEDULE_MAX_ACTIVITIES || alarmActive) return;
  if (scheduleActiveIndex == idx) return;
  if (scheduleActiveIndex >= 0) stopScheduleActivity();
  if(presetActive) stopPresetPlayback();
  schedulePreviousMode = displayMode;
  schedulePreviousCarousel = carouselActive || gifCarouselPlaybackFileActive;
  schedulePreviousCarouselSlot = carouselActiveSlot;
  ::memcpy(scheduleSavedFrame, framebuffer, LOGICAL_FRAME_BYTES);
  if (loadScheduleMedia(idx)) {
    scheduleActiveIndex = idx;
    scheduleFailedIndex = -1;
    if (scheduleGlobalFlags & 0x02) triggerScheduleStartBuzzer();
#if DEBUG_SERIAL
    Serial.print("SCH START i="); Serial.print(idx);
    Serial.print(" type="); Serial.print(scheduleActivities[idx].contentType);
    Serial.print(" window="); Serial.print(scheduleActivities[idx].startHour); Serial.print(':'); Serial.print(scheduleActivities[idx].startMinute);
    Serial.print('-'); Serial.print(scheduleActivities[idx].endHour); Serial.print(':'); Serial.println(scheduleActivities[idx].endMinute);
#endif
#if PNG_DIAG_SERIAL
    Serial.print("S+"); Serial.println(idx);
#endif
  } else {
    // Importante: senza questo latch updateSchedule() ritenterebbe il decode
    // ad ogni giro di loop, saturando CPU/heap e facendo sembrare la scheda bloccata.
    scheduleFailedIndex = (int8_t)idx;
#if DEBUG_SERIAL
    Serial.print("SCH START FAIL i="); Serial.print(idx);
    Serial.print(" type="); Serial.println(scheduleActivities[idx].contentType);
#endif
#if PNG_DIAG_SERIAL
    Serial.print("S!"); Serial.println(idx);
#endif
  }
}

void updateSchedule() {
  uint32_t nowMs = millis();
  updateScheduleMediaUploadTimeout();
  if (scheduleUploadOpen && scheduleUploadDirty && !scheduleHasActiveMediaUpload() &&
      (uint32_t)(nowMs - scheduleLastRxMs) >= SCHEDULE_COMMIT_DELAY_MS) {
    commitScheduleUpload();
  }
  if (!(scheduleGlobalFlags & 0x01) || alarmActive) {
    if (scheduleActiveIndex >= 0) stopScheduleActivity();
    return;
  }
#if RTC_ENABLED
  bool haveTime = (rtcReady && rtcTimeValid) || clockSynced;
#else
  bool haveTime = clockSynced;
#endif
  if (!haveTime) return;

  uint16_t y; uint8_t mo,d,h,mi,se;
  getAlarmDateTime(y,mo,d,h,mi,se);
  uint8_t dayBit = currentWeekdayBit(y,mo,d);
  uint16_t nowMin = (uint16_t)h * 60U + mi;
  int8_t wanted = -1;
  for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) {
    ScheduleActivity &a=scheduleActivities[i];
    if (!a.configured || !(a.flags & 0x01)) continue;
    if (!(a.flags & dayBit)) continue;
    if (scheduleTimeInside(a, nowMin)) { wanted=(int8_t)i; break; }
  }
  if (wanted < 0) {
    scheduleFailedIndex = -1;
    if (scheduleActiveIndex >= 0) stopScheduleActivity();
  } else if (scheduleActiveIndex != wanted && scheduleFailedIndex != wanted) {
    startScheduleActivity((uint8_t)wanted);
  }
}

bool handleScheduleCommand(const uint8_t *data, size_t len) {
  if (!data || len < 5) return false;

  // Global state: ACK 01.
  if (len == 5 && data[0] == 0x05 && data[1] == 0x00 &&
      data[2] == 0x07 && data[3] == 0x80) {
    uint8_t flags = data[4];
    if (!(flags & 0x01)) {
      scheduleUploadOpen = false;
      scheduleUploadDirty = false;
      scheduleReceivedMask = 0;
      cleanupScheduleTempFiles();
      clearScheduleMediaUploadStates();
      scheduleGlobalFlags = flags;
      scheduleStagingGlobalFlags = flags;
      bool persisted=saveScheduleGlobal();
      scheduleFailedIndex = -1;
      if (scheduleActiveIndex >= 0) stopScheduleActivity();

      Serial.print("SCH OFF f="); Serial.print(flags, HEX); Serial.print(" nvs="); Serial.println(persisted?1:0);
    } else {
      beginScheduleUpload(flags);
    }
    uint8_t ack[5] = {0x05,0x00,0x07,0x80,0x01};
    sendFA03(ack, sizeof(ack));
    return true;
  }

  // Activity media. Small/legacy activities still complete in one logical
  // packet. Large 64x64 activities repeat a 23-byte header for each logical
  // packet; mediaSize/mediaCRC describe the complete object and bytes from
  // offset 23 onward are the current chunk. BUILD 134 hardware validation
  // established ACK 01 as the continuation request for an incomplete object
  // and ACK 03 as the terminal ACK for the final CRC-valid chunk.
  if (len >= 23 && data[2] == 0x05 && data[3] == 0x80) {
    const uint16_t declared = rd16le(data);
    const uint8_t idx = data[4];
    const uint8_t chunkMarker = data[11];
    bool ok = declared == len && idx < SCHEDULE_MAX_ACTIVITIES && littleFsReady;
    ScheduleActivity candidate;
    size_t chunkSize = len - 23;

    if (ok) {
      candidate.configured = true;
      candidate.flags = data[5];
      candidate.startHour = data[6]; candidate.startMinute = data[7];
      candidate.endHour = data[8]; candidate.endMinute = data[9];
      candidate.contentType = data[10];
      candidate.mediaSize = rd32le(data + 12);
      candidate.mediaCRC = rd32le(data + 16);
      candidate.reserved = rd16le(data + 20);
      candidate.mediaId = data[22];
      ok = candidate.startHour < 24 && candidate.endHour < 24 &&
           candidate.startMinute < 60 && candidate.endMinute < 60 &&
           candidate.mediaSize > 0 && chunkSize > 0 &&
           (uint64_t)chunkSize <= (uint64_t)candidate.mediaSize;
    }

    if (ok) {
      const uint32_t bit = (1UL << idx);
      ScheduleMediaUploadState &u = scheduleMediaUploads[idx];

#if DEBUG_SERIAL
      Serial.print("SCH HDR i="); Serial.print(idx);
      Serial.print(" f=0x"); Serial.print(candidate.flags, HEX);
      Serial.print(" "); Serial.print(candidate.startHour); Serial.print(':'); Serial.print(candidate.startMinute);
      Serial.print("-"); Serial.print(candidate.endHour); Serial.print(':'); Serial.print(candidate.endMinute);
      Serial.print(" t="); Serial.print(candidate.contentType);
      Serial.print(" m=0x"); Serial.print(chunkMarker, HEX);
      Serial.print(" n="); Serial.print(candidate.mediaSize);
      Serial.print(" crc=0x"); Serial.print(candidate.mediaCRC, HEX);
      Serial.print(" r=0x"); Serial.print(candidate.reserved, HEX);
      Serial.print(" id=0x"); Serial.print(candidate.mediaId, HEX);
      Serial.print(" chunk="); Serial.println(chunkSize);
#endif

      // Reusing an already completed index means the app is replacing/retrying
      // that activity inside the same list upload. Start its staging file over.
      if (scheduleReceivedMask & bit) {
        scheduleReceivedMask &= ~bit;
        scheduleStaging[idx] = ScheduleActivity();
        resetScheduleMediaUpload(idx, true);
      } else if (u.active) {
        if (!scheduleMediaObjectMatches(u.candidate, candidate)) {
#if DEBUG_SERIAL
          Serial.print("SCH OBJECT CHANGE i="); Serial.print(idx);
          Serial.print(" old(t/n/crc)="); Serial.print(u.candidate.contentType); Serial.print('/');
          Serial.print(u.candidate.mediaSize); Serial.print("/0x"); Serial.print(u.candidate.mediaCRC, HEX);
          Serial.print(" new="); Serial.print(candidate.contentType); Serial.print('/');
          Serial.print(candidate.mediaSize); Serial.print("/0x"); Serial.println(candidate.mediaCRC, HEX);
#endif
          // A different media identity really is a new/restarted activity.
          resetScheduleMediaUpload(idx, true);
        } else if (!scheduleActivityHeaderMetadataMatches(u.candidate, candidate)) {
#if DEBUG_SERIAL
          // Ancillary timing/id fields may vary without changing the media
          // object. The per-chunk marker at offset 11 is intentionally excluded
          // from ScheduleActivity and from this comparison.
          Serial.print("SCH META VAR i="); Serial.print(idx);
          Serial.print(" f "); Serial.print(u.candidate.flags, HEX); Serial.print('>'); Serial.print(candidate.flags, HEX);
          Serial.print(" st "); Serial.print(u.candidate.startHour); Serial.print(':'); Serial.print(u.candidate.startMinute);
          Serial.print('>'); Serial.print(candidate.startHour); Serial.print(':'); Serial.print(candidate.startMinute);
          Serial.print(" en "); Serial.print(u.candidate.endHour); Serial.print(':'); Serial.print(u.candidate.endMinute);
          Serial.print('>'); Serial.print(candidate.endHour); Serial.print(':'); Serial.print(candidate.endMinute);
          Serial.print(" r "); Serial.print(u.candidate.reserved, HEX); Serial.print('>'); Serial.print(candidate.reserved, HEX);
          Serial.print(" id "); Serial.print(u.candidate.mediaId, HEX); Serial.print('>'); Serial.println(candidate.mediaId, HEX);
#endif
        }
      }

      if (!scheduleMediaUploads[idx].active)
        ok = beginScheduleMediaUpload(idx, candidate);
    }

    if (ok) {
      ScheduleMediaUploadState &u = scheduleMediaUploads[idx];
      if ((uint64_t)u.received + (uint64_t)chunkSize > (uint64_t)u.candidate.mediaSize) {
        ok = false;
      } else {
        File f = LittleFS.open(scheduleTempFileName(idx), "a");
        bool writeOK = (bool)f;
        if (writeOK && (uint32_t)f.size() != u.received) writeOK = false;
        if (writeOK && f.write(data + 23, chunkSize) != chunkSize) writeOK = false;
        if (f) f.close();
        if (!writeOK) {
          ok = false;
        } else {
          u.runningCRC = crc32Update(u.runningCRC, data + 23, chunkSize);
          u.received += (uint32_t)chunkSize;
          u.lastChunkAt = millis();
          // Keep the activity metadata from the first chunk. Later ancillary
          // header fields may be chunk markers; SCH META VAR records changes.
          scheduleUploadOpen = true;
          scheduleLastRxMs = u.lastChunkAt;

          if (u.received == u.candidate.mediaSize) {
            const uint32_t calc = u.runningCRC ^ 0xFFFFFFFFUL;
            if (calc != u.candidate.mediaCRC) {
#if DEBUG_SERIAL
              Serial.print("SCH CRC FAIL i="); Serial.print(idx);
              Serial.print(" calc=0x"); Serial.print(calc,HEX);
              Serial.print(" expected=0x"); Serial.println(u.candidate.mediaCRC,HEX);
#endif
              ok = false;
            } else {
              scheduleStaging[idx] = u.candidate;
              scheduleReceivedMask |= (1UL << idx);
              scheduleUploadDirty = true;
              u.active = false;
            }
          }
        }
      }
    }

    if (!ok) {
      // Schedule replacement is list-atomic by design. A malformed/failed media
      // chunk invalidates the new staging transaction but never touches the
      // currently active Schedule.
      abortScheduleUploadTransaction("activity media error");
    }

#if DEBUG_SERIAL
    if (ok) {
      const ScheduleMediaUploadState &u = scheduleMediaUploads[idx];
      const uint32_t total = (scheduleReceivedMask & (1UL << idx))
                               ? scheduleStaging[idx].mediaSize : u.received;
      const uint32_t expected = (scheduleReceivedMask & (1UL << idx))
                               ? scheduleStaging[idx].mediaSize : u.candidate.mediaSize;
      Serial.print("SCH RX i="); Serial.print(idx);
      Serial.print(" chunk="); Serial.print(chunkSize);
      Serial.print(" total="); Serial.print(total); Serial.print("/"); Serial.print(expected);
      Serial.print(" done="); Serial.println((scheduleReceivedMask & (1UL << idx)) ? 1 : 0);
    } else {
      Serial.print("SCH RX i="); Serial.print(idx);
      Serial.print(" chunk="); Serial.print(chunkSize);
      Serial.println(" ok=0");
    }
#endif

    // BUILD 134 hardware validation established the large-activity flow:
    // 0x01 requests the next chunk while the declared media object is incomplete,
    // and 0x03 terminates the CRC-valid final chunk. One-packet/small activities
    // therefore still receive the historically required 0x03 immediately.
    uint8_t scheduleStatus=0x02;
    if(ok){
      const bool complete=(scheduleReceivedMask & (1UL << idx)) != 0;
      scheduleStatus=complete ? (uint8_t)0x03 : (uint8_t)0x01;
#if DEBUG_SERIAL
      Serial.print("SCH ACK i="); Serial.print(idx);
      Serial.print(" status="); Serial.println(scheduleStatus,HEX);
#endif
    }
    uint8_t ack[5] = {0x05,0x00,0x05,0x80,scheduleStatus};
    sendFA03(ack, sizeof(ack));
    return true;
  }
  return false;
}

// ======================================================
// PERSISTENT RESET + BOOT POLICY
// ======================================================
void clearPersistentDeviceState() {
  abortAlarmUpload(nullptr);
  alarmPrefs.clear();
  for (uint8_t i=0;i<ALARM_SLOT_COUNT;i++) alarms[i]=AlarmSlot();

  schedulePrefs.clear();
  scheduleGlobalFlags=0; scheduleStagingGlobalFlags=0;
  scheduleUploadOpen=false; scheduleUploadDirty=false; scheduleReceivedMask=0;
  for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) {
    scheduleActivities[i]=ScheduleActivity();
    scheduleStaging[i]=ScheduleActivity();
  }

  stopPresetPlayback();
  clearPresetBank(true);

  if (carouselPrefsReady) carouselPrefs.clear();
  carouselOrderCount=0;
  for (uint8_t i=0;i<CAROUSEL_SLOT_COUNT;i++) {
    carouselOrder[i]=0;
    carouselSlots[i]=CarouselSlotMeta();
  }

  if (littleFsReady) {
    for (uint8_t i=0;i<ALARM_SLOT_COUNT;i++) {
      LittleFS.remove(alarmFileName(i));
      LittleFS.remove(String("/alarm")+i+".tmp");
      LittleFS.remove(String("/alarm")+i+".bak");
    }
    for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) {
      LittleFS.remove(scheduleFileName(i));
      LittleFS.remove(String("/sch")+i+".tmp");
      LittleFS.remove(String("/sch")+i+".bak");
    }
    for (uint8_t i=0;i<CAROUSEL_SLOT_COUNT;i++) {
      LittleFS.remove(carouselFileName(i,1));
      LittleFS.remove(carouselFileName(i,3));
      LittleFS.remove(carouselTempFileName(i));
      LittleFS.remove(carouselBackupFileName(i));
    }
    LittleFS.remove(GIF_PLAY_FILE); LittleFS.remove(GIF_PLAY_BACKUP_FILE);
    LittleFS.remove(GIF_RX_FILES[0]); LittleFS.remove(GIF_RX_FILES[1]);
    LittleFS.remove(EVENT_GIF_PLAY_FILE);
  }

  Preferences bp;
  if (bp.begin(BRIGHTNESS_NVS_NAMESPACE,false)) { bp.clear(); bp.end(); }
  brightnessPercent=100; brightnessDirty=false;
  energySaving=EnergySavingState();
  flipped180=false;
  diyMode=false;
  // Deliberately preserve clockSynced/syncMillis here: 03/80 is a live
  // device-state reset, not an electrical power cycle.
  applyCurrentBrightness();
#if DEBUG_SERIAL
  Serial.println("DEVICE RESET: persistent emulator state cleared (volatile time preserved)");
#endif
}

void applyBootDisplayPolicy() {
  presetActive=false; presetActiveSlot=-1; presetOrderCount=0; gifPresetPlaybackFileActive=false;
  carouselUploadBlackout=false; carouselUploadOpen=false; carouselStartPending=false;
  carouselActive=false; carouselActiveSlot=-1; carouselEnterRequested=false;
#if RTC_ENABLED
  if (rtcReady && rtcTimeValid) {
    screenOn=true; setStatusLed(true);
    displayMode=DISPLAY_CLOCK; clockCycleStartedAt=millis(); renderClock();
#if DEBUG_SERIAL
    Serial.println("BOOT POLICY: valid RTC -> CLOCK");
#endif
    return;
  }
#endif
  int8_t slot=nextCarouselSlot(-1);
  if (slot>=0) {
    screenOn=true; setStatusLed(true); carouselEnterRequested=true;
    if (startCarouselSlot((uint8_t)slot)) {
#if DEBUG_SERIAL
      Serial.print("BOOT POLICY: stored carousel -> slot "); Serial.println(slot);
#endif
      return;
    }
  }
  screenOn=false; setStatusLed(false); displayMode=DISPLAY_NONE;
  clearFramebuffer(); refreshMatrix();
#if DEBUG_SERIAL
  Serial.println("BOOT POLICY: no valid RTC/carousel -> SCREEN OFF");
#endif
}

#if OLED_STATUS_ENABLED
const char* oledModeName(uint8_t m) {
  switch(m) {
    case DISPLAY_SOLID: return "SOLID"; case DISPLAY_RAW: return "IMAGE";
    case DISPLAY_GRAFFITI: return "DIY"; case DISPLAY_GIF: return presetActive ? "PRESET" : (carouselActive ? "CAROUSEL" : "GIF");
    case DISPLAY_TEXT: return presetActive ? "PRESET" : (carouselActive ? "CAROUSEL" : "TEXT"); case DISPLAY_EFFECT: return "EFFECT";
    case DISPLAY_AUDIO: return "AUDIO"; case DISPLAY_CLOCK: return "CLOCK";
    case DISPLAY_COUNTDOWN: return "COUNTDOWN"; case DISPLAY_STOPWATCH: return "STOPWATCH";
    case DISPLAY_SCOREBOARD: return "SCORE"; default: return "IDLE";
  }
}

void setupStatusOLED() {
  // Same initialization as the hardware-validated DollaTek sketch, using rotation R0.
  statusOLED.begin();
  statusOLED.enableUTF8Print();
  statusOLED.setPowerSave(0);
  statusOLED.setContrast(50);
  statusOLEDReady=true;

  statusOLED.clearBuffer();
  statusOLED.setFont(u8g2_font_7x14B_tf);
  statusOLED.setCursor(0,14);
  statusOLED.print("iDotMatrix B");
  statusOLED.print(FW_BUILD);
  statusOLED.setCursor(0,31);
  statusOLED.print("OLED LIVE STATUS");
  statusOLED.setCursor(0,48);
  statusOLED.print("DollaTek ESP32");
  statusOLED.sendBuffer();
#if DEBUG_SERIAL
  Serial.println("OLED: U8g2 OK - rotation R0, pure event-driven dashboard");
#endif
  delay(1000);
}

void updateStatusOLED() {
  if(!statusOLEDReady) return;

  static bool first = true;
  static uint32_t lastUnknownAtSeen = 0;
  static bool lastBle = false;
  static bool lastScreen = false;
  static uint8_t lastMode = 0xFF;
  static uint8_t lastBrightness = 0xFF;
  static int8_t lastScheduleIndex = -127;
  static uint8_t lastScheduleFlags = 0xFF;
  static bool lastAlarmActive = false;
  static uint8_t lastAlarmSlot = 0xFE;
  static bool lastClockSynced = false;
  static bool lastStopwatchRunning = false;
  static bool lastCountdownRunning = false;
  static bool lastCountdownPaused = false;

  uint32_t now = millis();

  // When the alert expires, force an immediate redraw of the normal dashboard.
  bool alertExpired = false;
  if (unknownCommandActive && (uint32_t)(now - unknownCommandAt) >= OLED_UNKNOWN_ALERT_MS) {
    unknownCommandActive = false;
    alertExpired = true;
  }

  // Events that require an immediate OLED refresh.
  bool unknownChanged = unknownCommandActive && (unknownCommandAt != lastUnknownAtSeen);
  bool stateChanged = first || alertExpired || unknownChanged ||
                      deviceConnected != lastBle ||
                      screenOn != lastScreen ||
                      (uint8_t)displayMode != lastMode ||
                      brightnessPercent != lastBrightness ||
                      scheduleActiveIndex != lastScheduleIndex ||
                      scheduleGlobalFlags != lastScheduleFlags ||
                      alarmActive != lastAlarmActive ||
                      activeAlarmSlot != lastAlarmSlot ||
                      clockSynced != lastClockSynced ||
                      stopwatchRunning != lastStopwatchRunning ||
                      countdownRunning != lastCountdownRunning ||
                      countdownPaused != lastCountdownPaused;

  // BUILD 62: no periodic refresh. SW-I2C blocks the loop during
  // sendBuffer(), so the OLED is redrawn only after a real state change.
  if (!stateChanged) return;

  // Snapshot of displayed state: no sendBuffer() until the state changes.
  first = false;
  lastBle = deviceConnected;
  lastScreen = screenOn;
  lastMode = (uint8_t)displayMode;
  lastBrightness = brightnessPercent;
  lastScheduleIndex = scheduleActiveIndex;
  lastScheduleFlags = scheduleGlobalFlags;
  lastAlarmActive = alarmActive;
  lastAlarmSlot = activeAlarmSlot;
  lastClockSynced = clockSynced;
  lastStopwatchRunning = stopwatchRunning;
  lastCountdownRunning = countdownRunning;
  lastCountdownPaused = countdownPaused;
  if (unknownChanged) lastUnknownAtSeen = unknownCommandAt;

  statusOLED.clearBuffer();

  // Unhandled command: show an immediate alert for 8 seconds.
  if (unknownCommandActive) {
    char line[32];
    statusOLED.setFont(u8g2_font_7x14B_tf);
    statusOLED.drawStr(0,13,"UNKNOWN CMD!");
    statusOLED.setFont(u8g2_font_6x10_tf);
    snprintf(line,sizeof(line),"LEN:%u  N:%lu",unknownCommandLen,(unsigned long)unknownCommandCount);
    statusOLED.drawStr(0,25,line);

    for (uint8_t row=0; row<3; row++) {
      char hexline[24]; size_t pos=0;
      for (uint8_t j=0; j<4; j++) {
        uint8_t i=row*4+j;
        if (i>=unknownCommandStored) break;
        pos += snprintf(hexline+pos,sizeof(hexline)-pos,"%02X%s",unknownCommandData[i],j==3?"":" ");
      }
      statusOLED.drawStr(0,37+row*10,hexline);
    }
    statusOLED.sendBuffer();
    return;
  }

  statusOLED.setFont(u8g2_font_6x10_tf);
  char line[32];
  snprintf(line,sizeof(line),"B%d BLE:%s SCR:%s",FW_BUILD,deviceConnected?"ON":"OFF",screenOn?"ON":"OFF");
  statusOLED.drawStr(0,9,line);
  snprintf(line,sizeof(line),"MODE:%s",oledModeName(displayMode));
  statusOLED.drawStr(0,19,line);
  if(clockSynced) {
    uint8_t h,m,se; getCurrentTime(h,m,se);
    snprintf(line,sizeof(line),"%02u:%02u:%02u BRI:%u%%",h,m,se,brightnessPercent);
  } else snprintf(line,sizeof(line),"--:--:-- BRI:%u%%",brightnessPercent);
  statusOLED.drawStr(0,29,line);
  if(scheduleActiveIndex>=0) snprintf(line,sizeof(line),"SCH:%s ACT:%d",(scheduleGlobalFlags&1)?"ON":"OFF",scheduleActiveIndex);
  else snprintf(line,sizeof(line),"SCH:%s ACT:-",(scheduleGlobalFlags&1)?"ON":"OFF");
  statusOLED.drawStr(0,39,line);

  if(displayMode==DISPLAY_STOPWATCH) {
    uint32_t e=stopwatchElapsedMs+(stopwatchRunning?(now-stopwatchStartMillis):0);
    snprintf(line,sizeof(line),"SW %s %lu.%03lus",stopwatchRunning?"RUN":"STOP",(unsigned long)(e/1000),(unsigned long)(e%1000));
  } else if(displayMode==DISPLAY_COUNTDOWN) {
    uint32_t r=countdownRemainingMs;
    if(countdownRunning){uint32_t e=now-countdownStartMillis;r=e<r?r-e:0;}
    snprintf(line,sizeof(line),"CD %s %lus",countdownRunning?"RUN":(countdownPaused?"PAUSE":"STOP"),(unsigned long)((r+999)/1000));
  } else if(alarmActive) {
    snprintf(line,sizeof(line),"ALARM SLOT:%u UNK:%lu",activeAlarmSlot,(unsigned long)unknownCommandCount);
  } else {
    snprintf(line,sizeof(line),"UNK:%lu",(unsigned long)unknownCommandCount);
  }
  statusOLED.drawStr(0,49,line);
  snprintf(line,sizeof(line),"HEAP:%luK STK:%lu",(unsigned long)(ESP.getFreeHeap()/1024UL),(unsigned long)uxTaskGetStackHighWaterMark(NULL));
  statusOLED.drawStr(0,60,line);
  statusOLED.sendBuffer();
}
#endif

void setup(){
#if PNG_DIAG_SERIAL
  Serial.begin(115200);
#else
  DBG_BEGIN(115200);
#endif

  delay(300);
#if PNG_DIAG_SERIAL
  Serial.print("IDOTMATRIX_FW="); Serial.print(FW_RELEASE);
  Serial.print("-B"); Serial.println(FW_BUILD);
  Serial.print("BOOT rr="); Serial.print((int)esp_reset_reason());
  Serial.print(" heap="); Serial.println(ESP.getFreeHeap());
#endif
#if STATUS_LED_PIN >= 0
  pinMode(STATUS_LED_PIN,OUTPUT);
#endif
  setStatusLed(false);
#if OLED_STATUS_ENABLED
  setupStatusOLED();
#endif
#if ALARM_BUZZER_ENABLED && (ALARM_BUZZER_PIN >= 0)
  pinMode(ALARM_BUZZER_PIN,OUTPUT); digitalWrite(ALARM_BUZZER_PIN,BUZZER_ACTIVE_HIGH?LOW:HIGH);
#endif
// Allocate the logical display buffers at runtime. This is essential for the
  // 64x64 profile: three static 4096-pixel CRGB buffers would consume ~36 KB
  // of .bss and overflow the ESP32 DRAM linker segment.
  framebuffer = (CRGB*)malloc(LOGICAL_FRAME_BYTES);
  gifFrame = (CRGB*)malloc(LOGICAL_FRAME_BYTES);
  scheduleSavedFrame = (CRGB*)malloc(LOGICAL_FRAME_BYTES);
  if (!framebuffer || !gifFrame || !scheduleSavedFrame) {
#if DEBUG_SERIAL
    Serial.print("FATAL: logical framebuffer allocation failed, bytes each=");
    Serial.print(LOGICAL_FRAME_BYTES);
    Serial.print(" freeHeap=");
    Serial.println(ESP.getFreeHeap());
#endif
#if OLED_STATUS_ENABLED
    statusOLED.clearBuffer();
    statusOLED.setFont(u8g2_font_6x10_tf);
    statusOLED.drawStr(0, 14, "MEMORY ERROR");
    statusOLED.drawStr(0, 28, "Logical buffers");
    statusOLED.drawStr(0, 42, "allocation failed");
    statusOLED.sendBuffer();
#endif
    while (true) delay(1000);
  }
  fill_solid(framebuffer, NUM_LEDS, CRGB::Black);
  fill_solid(gifFrame, NUM_LEDS, CRGB::Black);
  fill_solid(scheduleSavedFrame, NUM_LEDS, CRGB::Black);
#if DEBUG_SERIAL
  Serial.print("LOGICAL BUFFERS: "); Serial.print(MATRIX_WIDTH); Serial.print('x'); Serial.print(MATRIX_HEIGHT);
  Serial.print(" x3, bytes="); Serial.print(LOGICAL_FRAME_BYTES * 3U);
  Serial.print(" freeHeap="); Serial.println(ESP.getFreeHeap());
  Serial.print("DISPLAY BACKEND: ");
#if DISPLAY_BACKEND == DISPLAY_BACKEND_HUB75
  Serial.println("HUB75 / MatrixPanel I2S DMA");
  Serial.print("HUB75 color depth: "); Serial.print(HUB75_COLOR_DEPTH); Serial.println(" bit/channel");
#else
  Serial.println("WS2812 / FastLED");
#endif
  Serial.print("DISPLAY SCALE: logical="); Serial.print(MATRIX_WIDTH); Serial.print('x'); Serial.print(MATRIX_HEIGHT);
  Serial.print(" physical="); Serial.print(PHYSICAL_MATRIX_WIDTH); Serial.print('x'); Serial.println(PHYSICAL_MATRIX_HEIGHT);
#if defined(BOARD_HAS_PSRAM)
  Serial.print("PSRAM: total="); Serial.print(ESP.getPsramSize()); Serial.print(" free="); Serial.println(ESP.getFreePsram());
#endif
#endif

#if RTC_ENABLED
  Wire.begin();
  rtcReady=rtc.begin();
  rtcTimeValid=rtcReady && !rtc.lostPower();
#if DEBUG_SERIAL
  Serial.print("RTC DS3231: "); Serial.println(rtcReady?"OK":"NOT FOUND");
  if(rtcReady && !rtcTimeValid) Serial.println("RTC: lost power; time invalid until BLE synchronization");
#endif
#endif
  // Always try a non-destructive mount first. The repository default enables
  // format-on-fail for first use/recovery; set it to 0 for forensic preservation.
#if DEBUG_SERIAL
  {
    const esp_partition_t *fsPart=esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if(fsPart){
      Serial.print("LittleFS partition: label="); Serial.print(fsPart->label);
      Serial.print(" address=0x"); Serial.print(fsPart->address,HEX);
      Serial.print(" size="); Serial.println(fsPart->size);
    } else {
      Serial.println("LittleFS partition: NOT FOUND (expected DATA/SPIFFS subtype)");
    }
  }
#endif
  littleFsReady=LittleFS.begin(false);
#if LITTLEFS_FORMAT_ON_MOUNT_FAIL
  if(!littleFsReady){
#if DEBUG_SERIAL
    Serial.println("LittleFS: mount failed; explicit format-on-fail enabled");
#endif
    // Reuse the Arduino-ESP32 mount-and-format path that older builds used,
    // but only after the explicit opt-in above.
    littleFsReady=LittleFS.begin(true);
  }
#endif
#if DEBUG_SERIAL
  Serial.print("LittleFS: "); Serial.println(littleFsReady ? "OK" : "ERROR (unavailable)");
  if(!littleFsReady){
#if LITTLEFS_FORMAT_ON_MOUNT_FAIL
    Serial.println("LittleFS hint: verify the Arduino partition scheme includes a SPIFFS/LittleFS-compatible data partition; FAT-only schemes are unsupported.");
#else
    Serial.println("LittleFS: set LITTLEFS_FORMAT_ON_MOUNT_FAIL=1 only for intentional recovery/first use");
    Serial.println("LittleFS hint: verify the Arduino partition scheme includes a SPIFFS/LittleFS-compatible data partition; FAT-only schemes are unsupported.");
#endif
  }
  if(littleFsReady){
    Serial.print("LittleFS capacity: used="); Serial.print(LittleFS.usedBytes());
    Serial.print(" total="); Serial.println(LittleFS.totalBytes());
  }
  Serial.print("ALARM SLOTS: "); Serial.println(ALARM_SLOT_COUNT);
#endif
  if(littleFsReady){
    recoverGifPlayFile();
    clearPresetBank(true); // Preset/Default slots are intentionally volatile across reboot.
  }
  // Preferences metadata is still loaded when LittleFS is offline so the
  // device can report/retain configuration; media recovery simply no-ops.
  loadAlarms();
  loadSchedule();
  loadCarousel();
#if DISPLAY_BACKEND == DISPLAY_BACKEND_WS2812
  FastLED.addLeds<LED_TYPE,MATRIX_PIN,COLOR_ORDER>(leds,PHYSICAL_NUM_LEDS);
#elif DISPLAY_BACKEND == DISPLAY_BACKEND_HUB75
  {
    HUB75_I2S_CFG mxconfig(PHYSICAL_MATRIX_WIDTH, PHYSICAL_MATRIX_HEIGHT, 1, hub75Pins);
    mxconfig.double_buff=false;
    // WLED maps its normal (non-reversed) MatrixPortal configuration to the
    // negative clock phase used successfully on this exact board/panel pair.
    mxconfig.clkphase=false;
    mxconfig.setPixelColorDepthBits(HUB75_COLOR_DEPTH);
    hub75Matrix=new MatrixPanel_I2S_DMA(mxconfig);
    const bool ok=hub75Matrix && hub75Matrix->begin();
#if DEBUG_SERIAL
    Serial.print("HUB75 MatrixPanel I2S DMA begin="); Serial.println(ok?"OK":"FAILED");
    Serial.print("HUB75 double buffer: OFF, clkphase: false, max brightness: "); Serial.println(MAX_LED_BRIGHTNESS);
#endif
    if(!ok){
#if DEBUG_SERIAL
      Serial.println("FATAL: HUB75 MatrixPanel I2S DMA initialization failed");
#endif
      while(true) delay(1000);
    }
  }
#endif
  loadBrightnessFromNVS(); clearPhysicalDisplay(); clearFramebuffer(); fill_solid(gifFrame,NUM_LEDS,CRGB::Black);

  runtimeStateMutex=xSemaphoreCreateMutex();
  if(!runtimeStateMutex){
#if DEBUG_SERIAL
    Serial.println("FATAL: runtime state mutex allocation failed");
#endif
#if OLED_STATUS_ENABLED
    statusOLED.clearBuffer();
    statusOLED.setFont(u8g2_font_6x10_tf);
    statusOLED.drawStr(0,14,"RUNTIME ERROR");
    statusOLED.drawStr(0,28,"Mutex allocation");
    statusOLED.drawStr(0,42,"failed");
    statusOLED.sendBuffer();
#endif
    while(true) delay(1000);
  }

  applyBootDisplayPolicy();

  BLEDevice::init(DEVICE_NAME);
  // Match the working WLED/NimBLE implementation and the official app's
  // large-transfer expectations. On Arduino-ESP32 3.3.x the compatibility
  // BLE layer otherwise negotiates an ATT payload of about 253 bytes. The app
  // appears to schedule long writes as if a 517-byte MTU were available; that
  // mismatch truncates 778-byte TEXT packets after 2 writes (506 bytes) and
  // 2890-byte media packets after 6 writes (1518 bytes).
  BLEDevice::setMTU(517);
#if DEBUG_SERIAL
  Serial.println("BLE local MTU configured: 517");
#endif
  server=BLEDevice::createServer(); server->setCallbacks(new ServerCallbacks());
  BLEService *fas=server->createService(FA_SERVICE_UUID);
  fa02=fas->createCharacteristic(FA02_UUID,BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_NR); fa02->setCallbacks(new FA02Callbacks());
  fa03=fas->createCharacteristic(FA03_UUID,BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY); fa03->addDescriptor(new BLE2902()); fas->start();
  BLEService *aes=server->createService(AE_SERVICE_UUID);
  ae01=aes->createCharacteristic(AE01_UUID,BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_NR); ae01->setCallbacks(new AE01Callbacks());
  ae02=aes->createCharacteristic(AE02_UUID,BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY); ae02->addDescriptor(new BLE2902()); aes->start();

  BLEAdvertising *adv=BLEDevice::getAdvertising(); BLEAdvertisementData ad;
  ad.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC|ESP_BLE_ADV_FLAG_BREDR_NOT_SPT); ad.setName(DEVICE_NAME); ad.setCompleteServices(BLEUUID(FA_SERVICE_UUID));
  const char mb[]={0x54,0x52,0x00,0x70,(char)IDOTMATRIX_SCREEN_TYPE,(char)FW_RELEASE_MAJOR,(char)FW_RELEASE_MINOR}; ad.setManufacturerData(String(mb,sizeof(mb))); adv->setAdvertisementData(ad);
  BLEAdvertisementData scan; scan.setCompleteServices(BLEUUID(AE_SERVICE_UUID)); adv->setScanResponseData(scan); adv->start();

  reportHeap("setup complete");
}

// ======================================================
// LOOP
// ======================================================
void loop(){
  bool restartAdvertisingNow=false;
  if(!lockRuntimeState()){ delay(1); return; }
  // BUILD 88: take the time snapshot only after the runtime mutex is held.
  // If loop() captures millis() before blocking on the mutex, a BLE callback
  // can update packetLastRxMs/bulkLastRxMs to a newer value while loop() waits.
  // Unsigned subtraction would then wrap and falsely look like a huge timeout.
  uint32_t now=millis();
  expireStalledTransfers(now);
  flushBrightnessSaveIfNeeded();
#if OLED_STATUS_ENABLED
  updateStatusOLED();
#endif
  if(pendingAdvertisingRestart && (long)(now-advertisingRestartAt)>=0){
    pendingAdvertisingRestart=false;
    restartAdvertisingNow=true;
  }
  if(pendingDeviceInfoPush && (long)(now-deviceInfoPushAt)>=0){pendingDeviceInfoPush=false;if(deviceConnected)sendDeviceInfo();}
  if(pendingSoftReset && (long)(now-softResetAt)>=0){pendingSoftReset=false;resetRuntimeState();}

  updateAlarms();
  updateSchedule();
  updateBuzzer();
  updateEnergySavingOutput(now);

  // B73: a decoder instance is never reused.  Switching GIFs is split across
  // separate loop iterations: first destroy the old AnimatedGIF object and
  // promote RX -> PLAY, then allocate/open a fresh decoder on the next pass.
  if(pendingGifStart && (long)(now-pendingGifStartAt)>=0 && !gifDecoderTeardownPending && !gifDecoderOpenPending){
    int8_t slot=pendingGifSlot;
    uint32_t newSize=pendingGifSize;
    pendingGifStart=false;
    pendingGifSlot=-1;
    pendingGifSize=0;
    if(slot>=0 && slot<=1){
      const char *rxPath=GIF_RX_FILES[(uint8_t)slot];
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
      Serial.print("GIF RX -> PLAY SWITCH slot="); Serial.print((int)slot);
      Serial.print(" bytes="); Serial.println(newSize);
      reportHeap("before GIF teardown");
#endif
      bool restartPrevious=(littleFsReady && displayMode==DISPLAY_GIF && gifStoredOnFS &&
                            !gifEventPlaybackFileActive && !gifCarouselPlaybackFileActive &&
                            LittleFS.exists(GIF_PLAY_FILE));
      uint32_t previousPlaySize=restartPrevious ? storedFileSize(GIF_PLAY_FILE) : 0;
      carouselActive=false; carouselActiveSlot=-1; carouselEnterRequested=false; carouselUploadOpen=false; carouselUploadBlackout=false; carouselStartPending=false;
      stopGIFPlayback();
      gifStoredOnFS=false; gifSize=0;

      bool promoted=promoteGifRxToPlay((uint8_t)slot);
      if(!promoted){
#if DEBUG_SERIAL
        Serial.println("GIF RX -> PLAY rename FAILED; previous PLAY preserved");
#endif
        if(restartPrevious && previousPlaySize && littleFsReady && LittleFS.exists(GIF_PLAY_FILE)){
          gifStoredOnFS=true;
          gifSize=previousPlaySize;
          gifDecoderOpenPending=true;
          gifDecoderOpenAt=millis()+1;
        }
      } else {
        gifStoredOnFS=true;
        gifSize=newSize;
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
        Serial.println("GIF RX -> PLAY rename OK");
        Serial.println("GIF OPEN deferred to next loop");
#endif
        gifDecoderOpenPending=true;
        gifDecoderOpenAt=millis()+1;
      }
    }
  }

  if(gifDecoderOpenPending && (long)(now-gifDecoderOpenAt)>=0){
    gifDecoderOpenPending=false;
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
    Serial.println("GIF FRESH DECODER START");
    reportHeap("before fresh GIF open");
#endif
    if(!startGIF()){
#if DEBUG_SERIAL
      Serial.println("GIF PLAY OPEN failed");
#endif
      destroyGIFDecoder();
      gifStoredOnFS=false;
      gifSize=0;
    } else {
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
      Serial.println("GIF PLAY START OK");
      reportHeap("after fresh GIF open");
#endif
    }
  }

  if(carouselTransferIndicatorActive) renderCarouselTransferIndicator(now);

#if APP_POWER_VERSION_SPLASH
  if(versionSplashActive && (int32_t)(now-versionSplashUntil)>=0){
    versionSplashActive=false;
    refreshMatrix();
  }
#endif

  if(!carouselTransferIndicatorActive && displayMode==DISPLAY_EFFECT) updateEffect();
  if(!carouselTransferIndicatorActive && displayMode==DISPLAY_AUDIO){ static uint32_t lastAudioRender=0; if(now-lastAudioRender>=80){ lastAudioRender=now; renderAudio(); } }
  updateCarousel(now);
  updatePreset(now);
  if(!carouselTransferIndicatorActive && displayMode==DISPLAY_GIF) updateGIF();
  if(!carouselTransferIndicatorActive && displayMode==DISPLAY_TEXT) updateTextAnimation();

  if(!carouselTransferIndicatorActive && displayMode==DISPLAY_CLOCK){ static uint32_t last=0; if(now-last>=100){last=now;renderClock();} }

  if(!carouselTransferIndicatorActive && displayMode==DISPLAY_COUNTDOWN){
    static uint32_t last=0; uint32_t remain=countdownRemainingMs;
    if(countdownRunning){ uint32_t e=now-countdownStartMillis; if(e>=countdownRemainingMs){remain=0;countdownRemainingMs=0;countdownRunning=false;if(!countdownFinishSent){countdownFinishSent=true;sendCommandStatus(0x08,0x80,0x03);triggerCountdownFinishBuzzer();}} else remain=countdownRemainingMs-e; }
    if(now-last>=200){
      last=now;
      renderCountdown(remain);
    }
  }

  if(!carouselTransferIndicatorActive && displayMode==DISPLAY_STOPWATCH){
    static uint32_t last=0; if(now-last>=200){last=now;uint32_t e=stopwatchElapsedMs;if(stopwatchRunning)e+=now-stopwatchStartMillis;renderStopwatch(e);}
  }

#if DEBUG_SERIAL
  static uint32_t lastHeap=0;
#if DEBUG_SERIAL && DEBUG_SERIAL_VERBOSE
  if(now-lastHeap>=30000){lastHeap=now;reportHeap("periodic");}
#else
  (void)lastHeap;
#endif
#endif
  unlockRuntimeState();
  if(restartAdvertisingNow) BLEDevice::startAdvertising();
  delay(5);
}
