#include <Arduino.h>
#include <esp_heap_caps.h>

// ======================================================
// FIRMWARE BUILD ID
// Incrementare ad ogni file consegnato: viene stampato
// sulla seriale all'avvio per evitare dubbi sulla versione.
// ======================================================
#define FW_BUILD 67
#define PNG_DIAG_SERIAL 0
#define TEXT_PROTOCOL_DEBUG 1
#define BULK_PROTOCOL_DEBUG 1

// Optional external RTC (DS3231 via RTClib).
// Keep 0 when no RTC hardware is installed: alarms use BLE time sync.
#define RTC_ENABLED             0
struct ScheduleActivity;
#define RTC_SYNC_FROM_BLE       1

// ======================================================
// DollaTek ESP32 OLED 0.96 / TTGO-style board
// ======================================================
#define DEBUG_SERIAL        1
#define OTA_ENABLED         0
#define MATRIX_PIN          17
#define STATUS_LED_PIN      25

// On-board SSD1306 diagnostic display. Set to 0 to compile it out completely.
#define OLED_STATUS_ENABLED 1
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
#if OLED_STATUS_ENABLED
  #include <U8g2lib.h>
  // Configurazione IDENTICA allo sketch DollaTek gia verificato dall'utente.
  U8G2_SSD1306_128X64_NONAME_F_SW_I2C statusOLED(OLED_ROTATION, OLED_SCL, OLED_SDA, OLED_RST);
  bool statusOLEDReady=false;
#endif

// PNG Schedule: usiamo l'inflater miniz gia presente nella ROM/SDK ESP32,
// cosi non serve aggiungere una libreria PNG esterna.
#if __has_include(<rom/miniz.h>)
  #include <rom/miniz.h>
#elif __has_include(<miniz.h>)
  #include <miniz.h>
#else
  #error "miniz header non trovato: richiesto per i PNG Schedule"
#endif
#if RTC_ENABLED
  #include <Wire.h>
  #include <RTClib.h>
  RTC_DS3231 rtc;
  bool rtcReady = false;
#endif

#if OTA_ENABLED
  #include <WiFi.h>
  #include <ArduinoOTA.h>
  #define WIFI_SSID       "TUO_WIFI"
  #define WIFI_PASSWORD   "TUA_PASSWORD"
  #define OTA_HOSTNAME    "idotmatrix-esp"
  #define OTA_PASSWORD    "idotmatrix-ota"
  bool otaReady = false;
  bool otaRunning = false;
#endif

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
// iDotMatrix logical screen profile.
// 1 = 16x16 (original development target)
// 3 = 32x32 (HXS-002 / NL-XSD-32, hardware-validated by community captures)
// The protocol/logical resolution is deliberately separated from the physical
// LED matrix so a 32x32 device can be emulated while still using a 16x16 panel
// as a downscaled preview.
// -----------------------------------------------------------------------------
#define IDOTMATRIX_SCREEN_TYPE  1

#if IDOTMATRIX_SCREEN_TYPE == 1
  #define MATRIX_WIDTH   16
  #define MATRIX_HEIGHT  16
#elif IDOTMATRIX_SCREEN_TYPE == 3
  #define MATRIX_WIDTH   32
  #define MATRIX_HEIGHT  32
#else
  #error "Unsupported IDOTMATRIX_SCREEN_TYPE (supported: 1=16x16, 3=32x32)"
#endif

#define NUM_LEDS       ((uint16_t)MATRIX_WIDTH * (uint16_t)MATRIX_HEIGHT)

// Physical LED panel connected to the ESP32.
// Keep these at 16x16 to emulate a 32x32 iDotMatrix with the existing matrix.
// Set them to 32x32 when a real 32x32 WS2812B panel is connected.
#define PHYSICAL_MATRIX_WIDTH   16
#define PHYSICAL_MATRIX_HEIGHT  16
#define PHYSICAL_NUM_LEDS       ((uint16_t)PHYSICAL_MATRIX_WIDTH * (uint16_t)PHYSICAL_MATRIX_HEIGHT)

#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB
#define MATRIX_MIRROR_X 1

// Abbassato come richiesto: 100% app = 64/255 FastLED.
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
  FastLED.setBrightness(hw);
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
#define MAX_TEXT_PAYLOAD    4096
#define MAX_TEXT_GLYPHS        64
#define TEXT_GLOBAL_HEADER     14
#define TEXT_GLYPH_META        4   // marker + RGB; glyph bitmap follows
#define TEXT_MAX_BITMAP_BYTES  64  // 16x32 glyph = 64 bytes
#define MAX_GIF_SIZE        (128UL * 1024UL)
#define MAX_EFFECT_COLORS   16

// ======================================================
// ALARMS
// ======================================================
#define ALARM_SLOT_COUNT       10
#define ALARM_BUZZER_ENABLED   1
#define ALARM_BUZZER_PIN       18
#define BUZZER_ACTIVE_HIGH     1

// Active-buzzer trill pattern: 3 short beeps followed by a longer pause.
// Fully non-blocking: BLE, animations and matrix refresh continue normally.
#define BUZZER_PULSE_ON_MS       90UL
#define BUZZER_PULSE_GAP_MS      70UL
#define BUZZER_TRILL_PAUSE_MS   550UL
#define BUZZER_TRILL_PULSES       3
#define ALARM_MEDIA_BASE_ID    0x14
#define ALARM_CONTENT_GIF      0x01
#define ALARM_CONTENT_RAW      0x02
#define ALARM_HEADER_SIZE      24

// ======================================================
// PROGRAM / SCHEDULE
// ======================================================
#define SCHEDULE_MAX_ACTIVITIES 32
#define SCHEDULE_COMMIT_DELAY_MS 900UL
#define SCHEDULE_CONTENT_GIF    0x01
#define SCHEDULE_CONTENT_IMAGE  0x02
#define SCHEDULE_CONTENT_TEXT   0x03
#define SCHEDULE_MEDIA_BASE_ID  0x1E
#define SCHEDULE_BUZZER_ENABLED 1
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

CRGB leds[PHYSICAL_NUM_LEDS];
CRGB framebuffer[NUM_LEDS];

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

// ======================================================
// ECO
// ======================================================
struct EnergySavingState {
  bool enabled = false;
  uint8_t startHour = 0, startMinute = 0;
  uint8_t endHour = 0, endMinute = 0;
  uint8_t reductionPercent = 0;
} energySaving;

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
  uint8_t glyphBytes = 16;
  uint8_t glyphAdvance = 8;
  int16_t offsetX = 0, offsetY = 1;
  uint32_t animationStart = 0;
  uint32_t lastFrame = 0;
} textState;

// ======================================================
// EFFECT LIGHT: 03 02 EFFECT SPEED COUNT [R G B]...
// RGB protocollo su scala 0..127.
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
extern uint8_t *gifData;
extern size_t gifWriteOffset;

bool allocateGIF(size_t size);
void freeGIF();
bool startGIF();
void switchDisplayMode(DisplayMode m);
void renderClock();


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

String alarmFileName(uint8_t slot) { return String("/alarm") + slot + ".bin"; }

void saveAlarmMeta(uint8_t slot) {
  if (slot >= ALARM_SLOT_COUNT) return;
  char key[12];
  snprintf(key,sizeof(key),"a%u",slot);
  alarmPrefs.putBytes(key,&alarms[slot],sizeof(AlarmSlot));
}

void loadAlarms() {
  alarmPrefs.begin("idot-alarm",false);
  for(uint8_t i=0;i<ALARM_SLOT_COUNT;i++){
    char key[12]; snprintf(key,sizeof(key),"a%u",i);
    size_t n=alarmPrefs.getBytesLength(key);
    if(n==sizeof(AlarmSlot)) alarmPrefs.getBytes(key,&alarms[i],sizeof(AlarmSlot));
    alarms[i].lastTriggerMinuteKey=0xFFFFFFFFUL;
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
      Serial.print(" file="); Serial.println(LittleFS.exists(alarmFileName(i))?"YES":"NO");
    }
#endif
  }
}

uint8_t currentWeekdayBit(uint16_t y,uint8_t m,uint8_t d){
  // Sakamoto: 0=Sunday. Protocollo: bit1=Monday ... bit7=Sunday.
  static const uint8_t t[]={0,3,2,5,0,3,5,1,4,6,2,4};
  uint16_t yy=y; if(m<3) yy--;
  uint8_t dow=(yy+yy/4-yy/100+yy/400+t[m-1]+d)%7;
  return dow==0 ? 0x80 : (uint8_t)(1U<<dow);
}

void getAlarmDateTime(uint16_t &y,uint8_t &mo,uint8_t &d,uint8_t &h,uint8_t &mi,uint8_t &se){
#if RTC_ENABLED
  if(rtcReady){
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
    bool leap=((y%4==0&&y%100!=0)||(y%400==0)); if(mo==2&&leap) dim=29;
    if(++d>dim){ d=1; if(++mo>12){mo=1;y++;} }
  }
}

bool loadAlarmMedia(uint8_t slot){
  if(slot>=ALARM_SLOT_COUNT) return false;
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
    if(!allocateGIF(a.mediaSize)){f.close();return false;}
    size_t got=f.read(gifData,a.mediaSize); f.close(); gifWriteOffset=got;
    if(got!=a.mediaSize){freeGIF();return false;}
    return startGIF();
  }
  f.close(); return false;
}

// ======================================================
// ACTIVE BUZZER - NON-BLOCKING TRILL
// ======================================================
#if ALARM_BUZZER_ENABLED && (ALARM_BUZZER_PIN >= 0)
static bool buzzerOutputOn = false;
static bool buzzerPatternRunning = false;
static uint8_t buzzerPulseIndex = 0;
static uint32_t buzzerNextChangeAt = 0;

static inline void setBuzzerOutput(bool on) {
  buzzerOutputOn = on;
  digitalWrite(ALARM_BUZZER_PIN,
               on ? (BUZZER_ACTIVE_HIGH ? HIGH : LOW)
                  : (BUZZER_ACTIVE_HIGH ? LOW : HIGH));
}

// Schedule state is defined later in the sketch; forward declarations are required here.
extern uint8_t scheduleGlobalFlags;
extern int8_t scheduleActiveIndex;

static bool buzzerRequested() {
  bool wanted = false;

  if (alarmActive && activeAlarmSlot < ALARM_SLOT_COUNT)
    wanted |= alarms[activeAlarmSlot].buzzer != 0;

#if SCHEDULE_BUZZER_ENABLED && (SCHEDULE_BUZZER_PIN >= 0)
  if (scheduleActiveIndex >= 0)
    wanted |= (scheduleGlobalFlags & 0x02) != 0;
#endif

  return wanted;
}

void updateBuzzer() {
  const bool wanted = buzzerRequested();
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
    ++buzzerPulseIndex;
    buzzerNextChangeAt = now +
      (buzzerPulseIndex >= BUZZER_TRILL_PULSES ? BUZZER_TRILL_PAUSE_MS
                                               : BUZZER_PULSE_GAP_MS);
  } else {
    if (buzzerPulseIndex >= BUZZER_TRILL_PULSES) buzzerPulseIndex = 0;
    setBuzzerOutput(true);
    buzzerNextChangeAt = now + BUZZER_PULSE_ON_MS;
  }
}
#else
void updateBuzzer() {}
#endif

void startAlarm(uint8_t slot){
  if(slot>=ALARM_SLOT_COUNT || alarmActive) return;
  AlarmSlot &a=alarms[slot];
  alarmPreviousMode=displayMode;
  alarmActive=true; activeAlarmSlot=slot; alarmEndsAt=millis()+(uint32_t)a.durationSec*1000UL;
  loadAlarmMedia(slot);
#if DEBUG_SERIAL
  Serial.print("ALARM TRIGGER slot=");Serial.print(slot);Serial.print(" duration=");Serial.print(a.durationSec);Serial.print(" buzzer=");Serial.println(a.buzzer);
#endif
}

void stopAlarm(){
  if(!alarmActive) return;
  alarmActive=false; activeAlarmSlot=0xFF;
  // Se il contenuto era GIF, la precedente GIF non e' piu' disponibile: torniamo all'orologio.
  switchDisplayMode(DISPLAY_CLOCK); renderClock();
}

void updateAlarms(){
  if(alarmActive){ if((int32_t)(millis()-alarmEndsAt)>=0) stopAlarm(); return; }
#if RTC_ENABLED
  if(!clockSynced && !rtcReady) return;
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
  AlarmSlot &a=alarms[slot];
  // Pacchetto corto: aggiorna/disarma i metadati senza media.
  if(len<ALARM_HEADER_SIZE){
    a.configured=true; a.flags=data[5]; a.hour=data[6]; a.minute=data[7]; a.durationSec=data[8];
    if(len>9)a.reserved1=data[9]; if(len>10)a.contentType=data[10]; if(len>11)a.buzzer=data[11];
    saveAlarmMeta(slot);
#if DEBUG_SERIAL
    Serial.print("ALARM META slot=");Serial.print(slot);Serial.print(" flags=0x");Serial.print(a.flags,HEX);Serial.print(" ");Serial.print(a.hour);Serial.print(":");Serial.println(a.minute);
#endif
    sendCommandAck(0x00,0x80); return true;
  }
  a.configured=true; a.flags=data[5]; a.hour=data[6]; a.minute=data[7]; a.durationSec=data[8];
  a.reserved1=data[9]; a.contentType=data[10]; a.buzzer=data[11]; a.reserved2=data[12];
  a.mediaSize=(uint32_t)data[13]|((uint32_t)data[14]<<8)|((uint32_t)data[15]<<16)|((uint32_t)data[16]<<24);
  a.mediaCRC=(uint32_t)data[17]|((uint32_t)data[18]<<8)|((uint32_t)data[19]<<16)|((uint32_t)data[20]<<24);
  a.reserved3=(uint16_t)data[21]|((uint16_t)data[22]<<8); a.mediaId=data[23];
  if(a.mediaSize > len-ALARM_HEADER_SIZE){
#if DEBUG_SERIAL
    Serial.println("ALARM ERROR: media size > packet");
#endif
    sendCommandAck(0x00,0x80); return true;
  }
  const uint8_t *media=data+ALARM_HEADER_SIZE;
  uint32_t calc=crc32Update(0xFFFFFFFF,media,a.mediaSize)^0xFFFFFFFF;
  if(calc!=a.mediaCRC){
#if DEBUG_SERIAL
    Serial.print("ALARM CRC ERROR calc=0x");Serial.print(calc,HEX);Serial.print(" expected=0x");Serial.println(a.mediaCRC,HEX);
#endif
    sendCommandAck(0x00,0x80); return true;
  }
  File f=LittleFS.open(alarmFileName(slot),"w");
  bool stored=f && f.write(media,a.mediaSize)==a.mediaSize; if(f)f.close();
  if(stored) saveAlarmMeta(slot);
#if DEBUG_SERIAL
  Serial.print("ALARM SAVE slot=");Serial.print(slot);Serial.print(" flags=0x");Serial.print(a.flags,HEX);
  Serial.print(" time=");Serial.print(a.hour);Serial.print(":");Serial.print(a.minute);Serial.print(" dur=");Serial.print(a.durationSec);
  Serial.print(" type=");Serial.print(a.contentType);Serial.print(" buzzer=");Serial.print(a.buzzer);Serial.print(" bytes=");Serial.print(a.mediaSize);
  Serial.print(" mediaId=0x");Serial.print(a.mediaId,HEX);Serial.print(" stored=");Serial.println(stored?"YES":"NO");
#endif
  sendCommandAck(0x00,0x80); return true;
}

// ======================================================
// GIF
// ======================================================
AnimatedGIF gif;
uint8_t *gifData = nullptr;
size_t gifSize = 0;
size_t gifWriteOffset = 0;
bool gifLoaded = false;
bool gifPlaying = false;
bool gifRestartPending = false;
bool gifFrameWasDrawn = false;
uint32_t gifNextFrameAt = 0;
CRGB gifFrame[NUM_LEDS];

// ======================================================
// PACKET / BULK
// ======================================================
uint8_t packetBuffer[MAX_PACKET_SIZE];
size_t packetReceived = 0;
size_t packetExpected = 0;

struct BulkTransferState {
  bool active = false;
  uint8_t dataType = 0;
  uint32_t expectedSize = 0;
  uint32_t receivedSize = 0;
  uint32_t expectedCRC = 0;
  uint32_t runningCRC = 0xFFFFFFFF;
  uint32_t chunkCount = 0;
  String format = "UNKNOWN";
} bulk;

uint8_t textPayload[MAX_TEXT_PAYLOAD];
size_t textPayloadReceived = 0;
uint8_t *rawRgbData = nullptr;
size_t rawRgbWriteOffset = 0;

bool pendingDeviceInfoPush = false;
uint32_t deviceInfoPushAt = 0;
bool pendingSoftReset = false;
uint32_t softResetAt = 0;

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

uint16_t logicalIndex(uint8_t x, uint8_t y) {
  return (uint16_t)y * MATRIX_WIDTH + x;
}

uint16_t physicalXY(uint8_t x, uint8_t y) {
  if ((y & 1) == 0) return (uint16_t)y * PHYSICAL_MATRIX_WIDTH + x;
  return (uint16_t)y * PHYSICAL_MATRIX_WIDTH + PHYSICAL_MATRIX_WIDTH - 1 - x;
}

void clearFramebuffer(const CRGB &color = CRGB::Black) {
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
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

void getCurrentTime(uint8_t &h, uint8_t &m, uint8_t &s) {
  if (!clockSynced) { h = m = s = 0; return; }
  uint32_t elapsed = (millis() - syncMillis) / 1000UL;
  uint32_t total = ((uint32_t)syncHour * 3600UL + (uint32_t)syncMinute * 60UL + syncSecond + elapsed) % 86400UL;
  h = total / 3600UL;
  total %= 3600UL;
  m = total / 60UL;
  s = total % 60UL;
}

bool isEnergySavingActive() {
  if (!energySaving.enabled || !clockSynced) return false;
  uint8_t h, m, s;
  getCurrentTime(h, m, s);
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

uint8_t brightnessToFastLED(uint8_t percent) {
  percent = min((uint8_t)100, percent);
  return ((uint16_t)percent * MAX_LED_BRIGHTNESS) / 100;
}

void refreshMatrix() {
  if (!screenOn) {
    FastLED.clear();
    FastLED.show();
    return;
  }
  FastLED.clear();
  // Render the logical iDotMatrix framebuffer onto the actually connected
  // matrix. If the sizes differ (e.g. logical 32x32 on physical 16x16), use
  // nearest-neighbour down/up-sampling. This is intentionally only a preview:
  // the BLE protocol still exposes the full logical resolution to the app.
  for (uint8_t py = 0; py < PHYSICAL_MATRIX_HEIGHT; py++) {
    for (uint8_t px = 0; px < PHYSICAL_MATRIX_WIDTH; px++) {
      uint8_t outX = px, outY = py;
#if MATRIX_MIRROR_X
      outX = PHYSICAL_MATRIX_WIDTH - 1 - outX;
#endif
      if (flipped180) {
        outX = PHYSICAL_MATRIX_WIDTH - 1 - outX;
        outY = PHYSICAL_MATRIX_HEIGHT - 1 - outY;
      }
      const uint8_t sx = (uint16_t)px * MATRIX_WIDTH / PHYSICAL_MATRIX_WIDTH;
      const uint8_t sy = (uint16_t)py * MATRIX_HEIGHT / PHYSICAL_MATRIX_HEIGHT;
      leds[physicalXY(outX, outY)] = framebuffer[logicalIndex(sx, sy)];
    }
  }
  FastLED.setBrightness(brightnessToFastLED(effectiveBrightnessPercent()));
  FastLED.show();
}

// ======================================================
// BLE TX
// ======================================================
void sendFA03(const uint8_t *data, size_t len) {
  if (!deviceConnected || !fa03) return;
  fa03->setValue(data, len);
  fa03->notify();
#if DEBUG_SERIAL
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
  uint8_t r[] = {0x09,0x00,0x01,0x80,0x04,0x0E,0x01,IDOTMATRIX_SCREEN_TYPE,0x00};
  sendFA03(r, sizeof(r));
}

// ======================================================
// DISPLAY MODE
// ======================================================
void stopGIFPlayback();
void switchDisplayMode(DisplayMode m) {
  if (displayMode == DISPLAY_GIF && m != DISPLAY_GIF) stopGIFPlayback();
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

void drawColon(int16_t x, int16_t y, const CRGB &c) {
  putPixel(x,y+1,c); putPixel(x,y+3,c);
}

void drawTimeTwoRows(uint8_t h, uint8_t m, const CRGB &hc, const CRGB &mc, bool leftColon=false) {
  drawDigit3x5(h/10, 4, 2, hc);
  drawDigit3x5(h%10, 8, 2, hc);
  drawDigit3x5(m/10, 4, 9, mc);
  drawDigit3x5(m%10, 8, 9, mc);
  drawColon(leftColon ? 2 : 12, 9, mc);
}

// ======================================================
// CLOCK STYLES - ricostruiti dal video dell'app.
// 0 rainbow frame + cifre colore selezionato
// 1 Christmas: rosso + albero verde
// 2 racing/checker: cyan/magenta, cifre arancio
// 3 fondo colore selezionato, cifre nere
// 4 hourglass: cifre colore selezionato + clessidra arancio/bianca
// 5 frame cyan/blue + cifre arancio
// 6 frame cyan/blue + cifre colore selezionato
// 7 frame quadranti RGBY + cifre colore selezionato
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
  // Albero 6x7 nella zona sinistra inferiore, come nel video.
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
  // Stile 3 del video: TRE BANDE PIENE sopra e sotto.
  // Esterna = azzurro, centrale = viola, interna = fuxia.
  // Non sono segmentate/scacchiera: ogni riga e' continua.
  const CRGB cyan(0,255,255);
  const CRGB violet(145,0,255);
  const CRGB fuchsia(255,0,170);

  for (uint8_t x=0; x<16; x++) {
    // sopra
    putPixel(x,0,cyan);
    putPixel(x,1,violet);
    putPixel(x,2,fuchsia);

    // sotto, speculare
    putPixel(x,13,fuchsia);
    putPixel(x,14,violet);
    putPixel(x,15,cyan);
  }
}

void drawFrameStyle5(bool cornerBlocks) {
  CRGB cyan(0,255,255), blue(0,70,255);

  if (cornerBlocks) {
    // Stile precedente: cornice azzurra con blocchi blu agli angoli.
    for (uint8_t x=2;x<=13;x++) { putPixel(x,1,cyan); putPixel(x,14,cyan); }
    for (uint8_t y=2;y<=13;y++) { putPixel(1,y,cyan); putPixel(14,y,cyan); }
    for (uint8_t y=0;y<3;y++) for (uint8_t x=0;x<3;x++) {
      putPixel(x,y,blue); putPixel(15-x,y,blue); putPixel(x,15-y,blue); putPixel(15-x,15-y,blue);
    }
  } else {
    // DUE cornici continue, entrambe larghe esattamente 1 pixel.
    // Esterno blu puro, interno azzurro/ciano puro.
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
  // 5x7 a sinistra, posizione del video.
  for (uint8_t x=0;x<=4;x++) { putPixel(x,8,orange); putPixel(x,14,orange); }
  putPixel(0,9,orange); putPixel(4,9,orange);
  putPixel(1,10,orange); putPixel(3,10,orange);
  putPixel(2,11,white);
  putPixel(1,12,white); putPixel(2,12,white); putPixel(3,12,white);
  putPixel(0,13,sand); putPixel(1,13,sand); putPixel(2,13,sand); putPixel(3,13,sand); putPixel(4,13,sand);
}

void renderClock() {
  uint8_t h,m,s;
  getCurrentTime(h,m,s);
  if (!clock24h) { if (h==0) h=12; else if (h>12) h-=12; }
  clearFramebuffer();

#if DEBUG_SERIAL
#endif

  switch (clockStyle & 0x07) {
    case 0: { // rainbow frame, selected digits
      drawRainbowBorder();
      drawTimeTwoRows(h,m,clockColor,clockColor,true);
      break;
    }
    case 1: { // Christmas
      CRGB red(255,0,0);
      drawDigit3x5(h/10,2,1,red); drawDigit3x5(h%10,6,1,red); drawColon(10,1,red);
      drawChristmasTree();
      drawDigit3x5(m/10,7,9,red); drawDigit3x5(m%10,11,9,red);
      break;
    }
    case 2: { // racing/checker
      drawCheckerRows();
      CRGB orange(255,170,0);
      drawDigit3x5(h/10,1,5,orange); drawDigit3x5(h%10,5,5,orange);
      putPixel(8,6,CRGB::White); putPixel(8,8,CRGB::White);
      drawDigit3x5(m/10,10,5,orange); drawDigit3x5(m%10,13,5,orange);
      break;
    }
    case 3: { // inverted blue/selected background
      clearFramebuffer(clockColor);
      CRGB black(0,0,0);
      drawTimeTwoRows(h,m,black,black,true);
      break;
    }
    case 4: { // hourglass
      drawDigit3x5(h/10,2,1,clockColor); drawDigit3x5(h%10,6,1,clockColor); drawColon(11,1,clockColor);
      drawHourglassIcon();
      drawDigit3x5(m/10,6,9,clockColor); drawDigit3x5(m%10,10,9,clockColor);
      break;
    }
    case 5: { // cyan rounded-ish frame + blue corners + orange digits
      drawFrameStyle5(true);
      CRGB orange(255,165,0);
      drawTimeTwoRows(h,m,orange,orange,true);
      break;
    }
    case 6: { // doppia cornice: blu esterna + azzurra interna
      // Disegniamo prima le cifre e POI le cornici, cosi' i due bordi
      // restano sempre completi e non possono essere sovrascritti.
      drawTimeTwoRows(h,m,clockColor,clockColor,true);
      drawFrameStyle5(false);
      break;
    }
    case 7: { // RGBY quadrant frame + selected digits
      drawQuadrantBorder();
      drawTimeTwoRows(h,m,clockColor,clockColor,true);
      break;
    }
  }

  scaleLegacy16CanvasToLogical();
  refreshMatrix();
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

void drawTextGlyphs(uint8_t scale=255,int16_t laserRow=-1) {
  for (uint8_t g=0;g<textState.glyphCount;g++) {
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
      uint16_t ph=e/100;
      for(uint8_t i=0;i<8;i++) putPixel((i*5+i*i*3)%MATRIX_WIDTH,(ph+i*3)%MATRIX_HEIGHT,CRGB::White);
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

void resetTextPosition() {
  int tw=textState.glyphCount*textState.glyphAdvance;
  int16_t centeredY=max((int16_t)0,(int16_t)(MATRIX_HEIGHT-textState.glyphHeight)/2);
  switch(textState.motionEffect){
    case 1:textState.offsetX=MATRIX_WIDTH;textState.offsetY=centeredY;break;
    case 2:textState.offsetX=-tw;textState.offsetY=centeredY;break;
    case 3:textState.offsetX=0;textState.offsetY=MATRIX_HEIGHT;break;
    case 4:textState.offsetX=0;textState.offsetY=-textState.glyphHeight;break;
    default:textState.offsetX=0;textState.offsetY=centeredY;break;
  }
  textState.animationStart=millis(); textState.lastFrame=0;
}

void updateTextAnimation() {
  if(!textState.valid) return;
  uint32_t now=millis();
  uint16_t interval=textFrameInterval();
  if(textState.motionEffect>=5 || textState.colorMode>=2) interval=min((uint16_t)45,interval);
  if(now-textState.lastFrame<interval) return;
  textState.lastFrame=now;
  int tw=textState.glyphCount*textState.glyphAdvance;
  switch(textState.motionEffect){
    case 1: if(--textState.offsetX < -tw) textState.offsetX=MATRIX_WIDTH; break;
    case 2: if(++textState.offsetX > MATRIX_WIDTH) textState.offsetX=-tw; break;
    case 3: if(--textState.offsetY < -(int16_t)textState.glyphHeight) textState.offsetY=MATRIX_HEIGHT; break;
    case 4: if(++textState.offsetY > MATRIX_HEIGHT) textState.offsetY=-textState.glyphHeight; break;
  }
  renderTextFrame();
}

// TEXT glyph records observed from the official app on the 32x32 profile:
// marker 0x02: 8x16 bitmap,  4-byte meta + 16 bitmap bytes = 20 bytes/glyph
// marker 0x05: 16x32 bitmap, 4-byte meta + 64 bitmap bytes = 68 bytes/glyph
// SimSun/SimHei are rasterized by the app: no device-side font selection is required.
void parseTextPayload(const uint8_t *data,size_t len) {
  if(len<TEXT_GLOBAL_HEADER) return;
  uint8_t requested=data[0];
  uint8_t n=min(requested,(uint8_t)MAX_TEXT_GLYPHS);
  if(!n) return;

  // Determine glyph format from the first record. Mixed-size records have not been observed.
  const uint8_t marker=data[TEXT_GLOBAL_HEADER];
  uint8_t glyphWidth=0,glyphHeight=0,glyphBytes=0;
  if(marker==0x02 || marker==0x03){ glyphWidth=8; glyphHeight=16; glyphBytes=16; }
  else if(marker==0x05 || marker==0x06){ glyphWidth=16; glyphHeight=32; glyphBytes=64; }
  else {
#if DEBUG_SERIAL
    Serial.print("TEXT unsupported glyph marker 0x"); Serial.println(marker,HEX);
#endif
    return;
  }
  const size_t recordBytes=TEXT_GLYPH_META+glyphBytes;
  const size_t need=TEXT_GLOBAL_HEADER+(size_t)requested*recordBytes;
  if(len<need) return;

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
  resetTextPosition(); switchDisplayMode(DISPLAY_TEXT); renderTextFrame();
}

// ======================================================
// LIGHT EFFECTS
// ======================================================
uint8_t effectChannelTo8(uint8_t v){ return v>=127 ? 255 : v*2; }
CRGB getEffectColor(uint8_t i){ if(!effectState.colorCount) return CRGB::White; return effectState.colors[i%effectState.colorCount]; }

CRGB effectPaletteGradient(uint8_t p){
  if(!effectState.colorCount) return CRGB::Black;
  if(effectState.colorCount==1) return effectState.colors[0];
  uint16_t scaled=(uint16_t)p*effectState.colorCount;
  uint8_t idx=scaled>>8, frac=scaled&0xFF;
  return blend(effectState.colors[idx%effectState.colorCount],effectState.colors[(idx+1)%effectState.colorCount],frac);
}

// Velocita' rallentata rispetto alla build precedente.
uint16_t effectFrameInterval(){
  uint8_t s=min((uint8_t)100,effectState.speed);

  // L'effetto 6 e' un vero cross-fade per-pixel: per vederlo
  // fluido serve un refresh frequente anche quando la velocita'
  // impostata nell'app e' molto bassa (es. speed=5).
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
    // Gradiente lungo: circa 10+ pixel per transizione colore.
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
  // Fondo: fading continuo fra i colori.
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++){
    uint8_t pos=(uint8_t)(ph+x*2+y*2);
    CRGB c=effectPaletteGradient(pos); c.nscale8_video(190);
    framebuffer[logicalIndex(x,y)]=c;
  }
  // Spike esclusivamente bianchi.
  uint32_t frame=ph/4;
  for(uint8_t i=0;i<18;i++){
    uint32_t h=effectHash((uint32_t)i*223UL+frame*19UL);
    putPixel((uint8_t)(h%MATRIX_WIDTH),(uint8_t)((h>>8)%MATRIX_HEIGHT),CRGB::White);
  }
}

void renderEffect3(){
  // L'originale avanza le bande a scatti di 2 pixel.
  uint32_t raw=effectPhase()/10;
  uint32_t ph=(raw/2)*2;
  uint8_t count=max((uint8_t)1,effectState.colorCount);
  const uint8_t stripeWidth=4;
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++)
    framebuffer[logicalIndex(x,y)]=getEffectColor(((x+ph)/stripeWidth)%count);
}

void renderEffect4(){
  // Anche le diagonali avanzano a scatti di 2 pixel.
  uint32_t raw=effectPhase()/10;
  uint32_t ph=(raw/2)*2;
  uint8_t count=max((uint8_t)1,effectState.colorCount);
  const uint8_t stripeWidth=4;
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++)
    framebuffer[logicalIndex(x,y)]=getEffectColor(((x+y+ph)/stripeWidth)%count);
}

void renderEffect5(){
  clearFramebuffer();
  uint32_t raw=effectPhase()/11;
  uint32_t ph=(raw/2)*2;
  uint8_t count=max((uint8_t)1,effectState.colorCount);
  const uint8_t colorWidth=5, blackWidth=4, block=colorWidth+blackWidth;
  for(uint8_t y=0;y<MATRIX_HEIGHT;y++) for(uint8_t x=0;x<MATRIX_WIDTH;x++){
    uint16_t d=x+y+ph; uint8_t within=d%block;
    if(within<colorWidth) putPixel(x,y,getEffectColor((d/block)%count));
  }
}

void renderEffect6(){
  // Effetto 7 UI: ogni pixel mantiene la propria fase e FADE
  // realmente da un colore al successivo. Non esiste alcuna
  // sostituzione istantanea del colore.
  const uint8_t count=max((uint8_t)1,effectState.colorCount);

  if(count==1){
    clearFramebuffer(getEffectColor(0));
    return;
  }

  // Nell'app speed=5 e' lento. A quella velocita' un singolo
  // passaggio dura circa 5.7 secondi; speed=100 scende a 700 ms.
  // Con update ogni 40 ms il fade ha molti step e deve risultare
  // chiaramente continuo anche a velocita' minima.
  const uint8_t sp=constrain(effectState.speed,0,100);
  const uint32_t fadeMs=map(sp,0,100,6000,700);
  const uint32_t elapsed=millis()-effectState.startMillis;
  const uint32_t cycleMs=fadeMs*(uint32_t)count;

  for(uint8_t y=0;y<MATRIX_HEIGHT;y++){
    for(uint8_t x=0;x<MATRIX_WIDTH;x++){
      const uint16_t i=(uint16_t)y*MATRIX_WIDTH+x;
      const uint32_t seed=effectHash((uint32_t)i*977UL+0x51EDUL);

      // Ogni pixel parte da un punto diverso dello stesso ciclo
      // continuo della palette.
      const uint32_t local=(elapsed+(seed%cycleMs))%cycleMs;
      const uint8_t a=(uint8_t)(local/fadeMs);
      const uint8_t b=(uint8_t)((a+1)%count);
      const uint32_t within=local%fadeMs;

      // 0..255 con risoluzione temporale elevata.
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
void stopGIFPlayback(){ gifPlaying=false; gifLoaded=false; gifRestartPending=false; gifFrameWasDrawn=false; gif.close(); }
void freeGIF(){ stopGIFPlayback(); if(gifData){ free(gifData); gifData=nullptr; } gifSize=gifWriteOffset=0; }
bool allocateGIF(size_t size){
  freeGIF(); if(!size || size>MAX_GIF_SIZE) return false;
  gifData=(uint8_t*)malloc(size); if(!gifData) return false;
  gifSize=size; gifWriteOffset=0; return true;
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

bool startGIF(){
  if(!gifData||!gifSize) return false;
  stopGIFPlayback(); fill_solid(gifFrame,NUM_LEDS,CRGB::Black);
  gif.begin(LITTLE_ENDIAN_PIXELS); gif.setDrawType(GIF_DRAW_RAW);
  if(!gif.open(gifData,(int)gifSize,GIFDraw)) return false;
  GIFINFO info; if(gif.getInfo(&info)==GIF_SUCCESS) gif.reset();
  gifLoaded=gifPlaying=true; gifNextFrameAt=millis(); displayMode=DISPLAY_GIF; return true;
}

void updateGIF(){
  if(!gifPlaying||!gifLoaded||displayMode!=DISPLAY_GIF) return;
  uint32_t now=millis(); if((long)(now-gifNextFrameAt)<0) return;
  if(gifRestartPending){ gif.reset(); gifRestartPending=false; fill_solid(gifFrame,NUM_LEDS,CRGB::Black); }
  gifFrameWasDrawn=false; int delayMs=0; int result=gif.playFrame(false,&delayMs);
  if(!gifFrameWasDrawn){ gif.reset(); gifNextFrameAt=now+10; return; }
  ::memcpy(framebuffer,gifFrame,sizeof(framebuffer)); refreshMatrix();
  if(delayMs<10) delayMs=10; gifNextFrameAt=now+delayMs; if(result==0) gifRestartPending=true;
}

// ======================================================
// BULK
// ======================================================
bool startsWithGIF(const uint8_t *data,size_t len){ return len>=6 && (!memcmp(data,"GIF87a",6)||!memcmp(data,"GIF89a",6)); }
void resetBulkTransfer(){
  bulk=BulkTransferState();
  textPayloadReceived=0;
  if(rawRgbData){ free(rawRgbData); rawRgbData=nullptr; }
  rawRgbWriteOffset=0;
}

bool processBulkPacket(const uint8_t *data,size_t len){
  if(len<16 || data[3]!=0x00 || data[2]>0x03) return false;
  uint8_t type=data[2];
  uint32_t total=(uint32_t)data[5]|((uint32_t)data[6]<<8)|((uint32_t)data[7]<<16)|((uint32_t)data[8]<<24);
  uint32_t crc=(uint32_t)data[9]|((uint32_t)data[10]<<8)|((uint32_t)data[11]<<16)|((uint32_t)data[12]<<24);
  if(!total || total>10UL*1024UL*1024UL) return false;
  const uint8_t *payload=data+16; size_t payloadSize=len-16;
  if(!bulk.active){
    resetBulkTransfer(); bulk.active=true; bulk.dataType=type; bulk.expectedSize=total; bulk.expectedCRC=crc; bulk.runningCRC=0xFFFFFFFF;
    if(startsWithGIF(payload,payloadSize)) bulk.format="GIF";
    else if(type==2&&total==(uint32_t)NUM_LEDS*3UL) bulk.format="RAW RGB";
    else if(type==3) bulk.format="TEXT";
#if DEBUG_SERIAL && BULK_PROTOCOL_DEBUG
    Serial.print("BULK BEGIN type="); Serial.print(type);
    Serial.print(" total="); Serial.print(total);
    Serial.print(" firstPayload="); Serial.print(payloadSize);
    Serial.print(" format="); Serial.println(bulk.format.length() ? bulk.format : "UNKNOWN");
#endif
    if(type==1 && !allocateGIF(total)){ resetBulkTransfer(); sendTransferAck(type,0x03); return true; }
    if(type==2 && bulk.format=="RAW RGB"){
      rawRgbData=(uint8_t*)malloc(total);
      if(!rawRgbData){ resetBulkTransfer(); sendTransferAck(type,0x03); return true; }
      rawRgbWriteOffset=0;
    }
  }
  if(type!=bulk.dataType){ resetBulkTransfer(); return true; }
  uint32_t remain=bulk.expectedSize-bulk.receivedSize; size_t useful=min((size_t)remain,payloadSize);
  bulk.runningCRC=crc32Update(bulk.runningCRC,payload,useful);
  if(type==1 && gifData && gifWriteOffset+useful<=gifSize){ ::memcpy(gifData+gifWriteOffset,payload,useful); gifWriteOffset+=useful; }
  if(type==2 && rawRgbData && rawRgbWriteOffset+useful<=bulk.expectedSize){
    ::memcpy(rawRgbData+rawRgbWriteOffset,payload,useful);
    rawRgbWriteOffset+=useful;
  }
  if(type==3){
    size_t off=bulk.receivedSize;
    if(off<MAX_TEXT_PAYLOAD){ size_t cp=min(useful,(size_t)MAX_TEXT_PAYLOAD-off); ::memcpy(textPayload+off,payload,cp); textPayloadReceived=off+cp; }
  }
  bulk.receivedSize+=useful; bulk.chunkCount++;
  if(bulk.receivedSize>=bulk.expectedSize){
    bool ok=((bulk.runningCRC^0xFFFFFFFF)==bulk.expectedCRC);
#if DEBUG_SERIAL && TEXT_PROTOCOL_DEBUG
    if(type==3){
      Serial.print("TEXT RX COMPLETE len="); Serial.print(textPayloadReceived);
      Serial.print(" crc="); Serial.println(ok ? "OK" : "BAD");
    }
#endif
    if(type==3 && ok && textPayloadReceived) parseTextPayload(textPayload,textPayloadReceived);
    if(type==1){ if(ok && gifWriteOffset==gifSize) startGIF(); else freeGIF(); }
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
    Serial.print(" crc="); Serial.println(ok ? "OK" : "BAD");
#endif
    sendTransferAck(type,0x03); resetBulkTransfer();
  } else sendTransferAck(type,0x01);
  return true;
}

// ======================================================
// AUDIO / RHYTHM - BUILD 36 RENDERER REFINEMENT
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
    syncYear=2000+data[4]; syncMonth=data[5]; syncDay=data[6]; syncHour=data[8]; syncMinute=data[9]; syncSecond=data[10]; syncMillis=millis(); clockSynced=true;
#if RTC_ENABLED && RTC_SYNC_FROM_BLE
    if(rtcReady){
      rtc.adjust(DateTime(syncYear,syncMonth,syncDay,syncHour,syncMinute,syncSecond));
#if DEBUG_SERIAL
      Serial.println("RTC: synchronized from BLE time");
#endif
    }
#endif
    sendCommandAck(0x01,0x80); return;
  }

  if(len==5 && cmd==0x07 && sub==0x01){ screenOn=data[4]!=0; setStatusLed(screenOn); refreshMatrix(); sendCommandAck(cmd,sub); return; }
  if(len==5 && cmd==0x06 && sub==0x80){ flipped180=data[4]!=0; refreshMatrix(); sendCommandAck(cmd,sub); return; }

  if(len==10 && cmd==0x02 && sub==0x80){
    energySaving.enabled=data[4]!=0; energySaving.startHour=data[5]; energySaving.startMinute=data[6]; energySaving.endHour=data[7]; energySaving.endMinute=data[8]; energySaving.reductionPercent=data[9];
    refreshMatrix(); sendCommandAck(cmd,sub); return;
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
    switchDisplayMode(DISPLAY_CLOCK); renderClock(); sendCommandAck(cmd,sub); return;
  }

  if(len==7 && cmd==0x08 && sub==0x80){
    uint8_t mode=data[4]; uint32_t req=((uint32_t)data[5]*60UL+data[6])*1000UL;
    switch(mode){
      case 0:countdownRunning=false;countdownPaused=false;countdownRemainingMs=0;countdownFinishSent=false;break;
      case 1:countdownRemainingMs=req;countdownStartMillis=millis();countdownRunning=true;countdownPaused=false;countdownFinishSent=false;break;
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

  if (handleScheduleCommand(data, len)) return;

  // Unknown command: always latch it for the OLED, even when Serial debug is off.
  unknownCommandActive = true;
  unknownCommandAt = millis();
  unknownCommandCount++;
  unknownCommandLen = (uint16_t)min(len, (size_t)0xFFFF);
  unknownCommandStored = (uint8_t)min(len, (size_t)OLED_UNKNOWN_BYTES);
  for (uint8_t i=0; i<unknownCommandStored; i++) unknownCommandData[i]=data[i];
#if DEBUG_SERIAL
  Serial.print("COMANDO NON GESTITO [");Serial.print(len);Serial.print("]: ");dumpHex(data,len);
#endif
  sendCommandAck(cmd,sub);
}

// ======================================================
// BLE CALLBACKS
// ======================================================
class FA02Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    String value=c->getValue(); if(!value.length()) return;
    const uint8_t *data=(const uint8_t*)value.c_str(); size_t len=value.length();
    if(packetReceived==0){ if(len<2)return; packetExpected=(uint16_t)data[0]|((uint16_t)data[1]<<8); if(!packetExpected||packetExpected>MAX_PACKET_SIZE){packetExpected=packetReceived=0;return;} }
    if(packetReceived+len>MAX_PACKET_SIZE){packetExpected=packetReceived=0;resetBulkTransfer();return;}
    ::memcpy(packetBuffer+packetReceived,data,len); packetReceived+=len;
    if(packetReceived>=packetExpected){ processFA02Packet(packetBuffer,packetExpected); packetReceived=packetExpected=0; }
  }
};

class AE01Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
#if DEBUG_SERIAL
    String v=c->getValue(); if(!v.length()) return; Serial.print("RX AE01 [");Serial.print(v.length());Serial.print("]: ");dumpHex((const uint8_t*)v.c_str(),v.length());
#endif
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
#if PNG_DIAG_SERIAL
    Serial.println("BLE C");
#endif
    deviceConnected=true; screenOn=true; setStatusLed(true); refreshMatrix(); pendingDeviceInfoPush=true; deviceInfoPushAt=millis()+1200;
  }
  void onDisconnect(BLEServer*) override {
#if PNG_DIAG_SERIAL
    Serial.println("BLE D");
#endif
    deviceConnected=false; resetBulkTransfer(); delay(300); BLEDevice::startAdvertising();
  }
};

// ======================================================
// OTA
// ======================================================
#if OTA_ENABLED
void setupOTA(){
  WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  uint32_t t0=millis(); while(WiFi.status()!=WL_CONNECTED && millis()-t0<15000) delay(250);
  if(WiFi.status()!=WL_CONNECTED){WiFi.disconnect(true);WiFi.mode(WIFI_OFF);return;}
  if(ESP.getFreeHeap()<25000){WiFi.disconnect(true);WiFi.mode(WIFI_OFF);return;}
  ArduinoOTA.setHostname(OTA_HOSTNAME); ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([](){otaRunning=true;}); ArduinoOTA.onEnd([](){otaRunning=false;}); ArduinoOTA.onError([](ota_error_t){otaRunning=false;});
  ArduinoOTA.begin(); otaReady=true;
}
#endif

// ======================================================
// RESET RUNTIME
// ======================================================
void resetRuntimeState(){
  stopGIFPlayback(); diyMode=false; effectState.valid=false; textState.valid=false; countdownRunning=false; stopwatchRunning=false; scoreA=scoreB=0; displayMode=DISPLAY_NONE; clearFramebuffer(); refreshMatrix();
}

// ======================================================
// SETUP
// ======================================================


// -----------------------------------------------------------------------------
// PROGRAM / SCHEDULE - BUILD 44
// L'app conserva i programmi; il device conserva solamente la schedule attiva.
// 07 80 -> stato globale (bit0 enabled, bit1 sound), ACK=01
// 05 80 -> attivita completa, ACK=03
// Una nuova lista viene ricevuta in staging e committata dopo un breve timeout.
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
bool scheduleUploadOpen = false;
bool scheduleUploadDirty = false;
uint32_t scheduleLastRxMs = 0;
uint32_t scheduleReceivedMask = 0;
int8_t scheduleActiveIndex = -1;
int8_t scheduleFailedIndex = -1;  // evita retry continuo se un media non viene decodificato
DisplayMode schedulePreviousMode = DISPLAY_CLOCK;
CRGB scheduleSavedFrame[NUM_LEDS];

static inline uint16_t rd16le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t rd32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

String scheduleFileName(uint8_t idx) { return String("/sch") + idx + ".bin"; }
String scheduleTempFileName(uint8_t idx) { return String("/scht") + idx + ".bin"; }

void saveScheduleGlobal() {
  schedulePrefs.putUChar("flags", scheduleGlobalFlags);
}

void saveScheduleMeta(uint8_t idx) {
  if (idx >= SCHEDULE_MAX_ACTIVITIES) return;
  char key[10]; snprintf(key, sizeof(key), "s%u", idx);
  schedulePrefs.putBytes(key, &scheduleActivities[idx], sizeof(ScheduleActivity));
}

void clearScheduleMeta(uint8_t idx) {
  if (idx >= SCHEDULE_MAX_ACTIVITIES) return;
  char key[10]; snprintf(key, sizeof(key), "s%u", idx);
  schedulePrefs.remove(key);
  scheduleActivities[idx] = ScheduleActivity();
  LittleFS.remove(scheduleFileName(idx));
}

void loadSchedule() {
  schedulePrefs.begin("idot-sched", false);
  scheduleGlobalFlags = schedulePrefs.getUChar("flags", 0);
  uint8_t count = 0;
  for (uint8_t i = 0; i < SCHEDULE_MAX_ACTIVITIES; i++) {
    char key[10]; snprintf(key, sizeof(key), "s%u", i);
    size_t n = schedulePrefs.getBytesLength(key);
    if (n == sizeof(ScheduleActivity)) {
      schedulePrefs.getBytes(key, &scheduleActivities[i], sizeof(ScheduleActivity));
      if (scheduleActivities[i].configured) count++;
    }
  }

  Serial.print("SCH LOAD "); Serial.print(scheduleGlobalFlags, HEX); Serial.print("/"); Serial.println(count);
}

bool writeScheduleTemp(uint8_t idx, const uint8_t *payload, uint32_t size, uint32_t expectedCRC) {
  if (idx >= SCHEDULE_MAX_ACTIVITIES || !payload || !size) return false;
  uint32_t calc = crc32Update(0xFFFFFFFF, payload, size) ^ 0xFFFFFFFF;
  if (calc != expectedCRC) {

    Serial.print("SCH CRC FAIL i="); Serial.println(idx);
    return false;
  }
  File f = LittleFS.open(scheduleTempFileName(idx), "w");
  if (!f) return false;
  size_t wrote = f.write(payload, size);
  f.close();
  return wrote == size;
}

void beginScheduleUpload(uint8_t flags) {
  scheduleGlobalFlags = flags;
  saveScheduleGlobal();
  scheduleUploadOpen = true;
  scheduleUploadDirty = false;
  scheduleFailedIndex = -1;
  scheduleReceivedMask = 0;
  scheduleLastRxMs = millis();
  for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) {
    scheduleStaging[i] = ScheduleActivity();
    LittleFS.remove(scheduleTempFileName(i));
  }

  Serial.print("SCH OPEN "); Serial.println(flags, HEX);
}

void commitScheduleUpload() {
  if (!scheduleUploadOpen || !scheduleUploadDirty) {
    scheduleUploadOpen = false;
    return;
  }
  for (uint8_t i=0;i<SCHEDULE_MAX_ACTIVITIES;i++) {
    bool received = (scheduleReceivedMask & (1UL << i)) != 0;
    if (received) {
      scheduleActivities[i] = scheduleStaging[i];
      String tmp = scheduleTempFileName(i), dst = scheduleFileName(i);
      LittleFS.remove(dst);
      if (LittleFS.exists(tmp)) LittleFS.rename(tmp, dst);
      saveScheduleMeta(i);
    } else if (scheduleActivities[i].configured) {
      clearScheduleMeta(i);
    }
  }
  scheduleUploadOpen = false;
  scheduleUploadDirty = false;

  Serial.print("SCH COMMIT "); Serial.println(scheduleReceivedMask, HEX);
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
  // lo stato dell'inflater sull'heap e chiamiamo l'API low-level.
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
  if (idx >= SCHEDULE_MAX_ACTIVITIES) return false;
  ScheduleActivity &a = scheduleActivities[idx];
  if (!a.configured || !a.mediaSize) return false;
  File f = LittleFS.open(scheduleFileName(idx), "r");
  if (!f || (uint32_t)f.size() != a.mediaSize) { if(f)f.close(); return false; }

  if (a.contentType == SCHEDULE_CONTENT_GIF) {
    if (!allocateGIF(a.mediaSize)) { f.close(); return false; }
    size_t got = f.read(gifData, a.mediaSize); f.close();
    gifWriteOffset = got;
    if (got != a.mediaSize) { freeGIF(); return false; }
    return startGIF();
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
  if (schedulePreviousMode == DISPLAY_SOLID || schedulePreviousMode == DISPLAY_RAW ||
      schedulePreviousMode == DISPLAY_GRAFFITI) {
    ::memcpy(framebuffer, scheduleSavedFrame, sizeof(framebuffer));
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
  schedulePreviousMode = displayMode;
  ::memcpy(scheduleSavedFrame, framebuffer, sizeof(framebuffer));
  if (loadScheduleMedia(idx)) {
    scheduleActiveIndex = idx;
    scheduleFailedIndex = -1;

#if PNG_DIAG_SERIAL
    Serial.print("S+"); Serial.println(idx);
#endif
  } else {
    // Importante: senza questo latch updateSchedule() ritenterebbe il decode
    // ad ogni giro di loop, saturando CPU/heap e facendo sembrare la scheda bloccata.
    scheduleFailedIndex = (int8_t)idx;
#if PNG_DIAG_SERIAL
    Serial.print("S!"); Serial.println(idx);
#endif
  }
}

void updateSchedule() {
  uint32_t nowMs = millis();
  if (scheduleUploadOpen && scheduleUploadDirty &&
      (uint32_t)(nowMs - scheduleLastRxMs) >= SCHEDULE_COMMIT_DELAY_MS) {
    commitScheduleUpload();
  }
  if (!(scheduleGlobalFlags & 0x01) || alarmActive) {
    if (scheduleActiveIndex >= 0) stopScheduleActivity();
    return;
  }
#if RTC_ENABLED
  bool haveTime = rtcReady || clockSynced;
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

  // Stato globale: ACK 01
  if (len == 5 && data[0] == 0x05 && data[1] == 0x00 &&
      data[2] == 0x07 && data[3] == 0x80) {
    uint8_t flags = data[4];
    if (!(flags & 0x01)) {
      scheduleGlobalFlags = flags;
      saveScheduleGlobal();
      scheduleUploadOpen = false;
      scheduleUploadDirty = false;
      scheduleFailedIndex = -1;
      if (scheduleActiveIndex >= 0) stopScheduleActivity();

      Serial.print("SCH OFF f="); Serial.println(flags, HEX);
    } else {
      beginScheduleUpload(flags);
    }
    uint8_t ack[5] = {0x05,0x00,0x07,0x80,0x01};
    sendFA03(ack, sizeof(ack));
    return true;
  }

  // Attivita completa: ACK 03
  if (len >= 23 && data[2] == 0x05 && data[3] == 0x80) {
    uint16_t declared = rd16le(data);
    uint8_t idx = data[4];
    uint32_t payloadSize = rd32le(data + 12);
    bool ok = declared == len && (23UL + payloadSize) == len && idx < SCHEDULE_MAX_ACTIVITIES;
    if (ok) {
      ScheduleActivity a;
      a.configured = true;
      a.flags = data[5];
      a.startHour = data[6]; a.startMinute = data[7];
      a.endHour = data[8]; a.endMinute = data[9];
      a.contentType = rd16le(data + 10);
      a.mediaSize = payloadSize;
      a.mediaCRC = rd32le(data + 16);
      a.reserved = rd16le(data + 20);
      a.mediaId = data[22];
      ok = writeScheduleTemp(idx, data + 23, payloadSize, a.mediaCRC);
      if (ok) {
        scheduleStaging[idx] = a;
        scheduleReceivedMask |= (1UL << idx);
        scheduleUploadOpen = true;
        scheduleUploadDirty = true;
        scheduleLastRxMs = millis();
      }
    }

    Serial.print("SCH RX i="); Serial.print(idx);
    Serial.print(" ok="); Serial.println(ok ? 1 : 0);
    uint8_t ack[5] = {0x05,0x00,0x05,0x80, ok ? (uint8_t)0x03 : (uint8_t)0x02};
    sendFA03(ack, sizeof(ack));
    return true;
  }
  return false;
}

#if OLED_STATUS_ENABLED
const char* oledModeName(uint8_t m) {
  switch(m) {
    case DISPLAY_SOLID: return "SOLID"; case DISPLAY_RAW: return "IMAGE";
    case DISPLAY_GRAFFITI: return "DIY"; case DISPLAY_GIF: return "GIF";
    case DISPLAY_TEXT: return "TEXT"; case DISPLAY_EFFECT: return "EFFECT";
    case DISPLAY_AUDIO: return "AUDIO"; case DISPLAY_CLOCK: return "CLOCK";
    case DISPLAY_COUNTDOWN: return "COUNTDOWN"; case DISPLAY_STOPWATCH: return "STOPWATCH";
    case DISPLAY_SCOREBOARD: return "SCORE"; default: return "IDLE";
  }
}

void setupStatusOLED() {
  // Stessa inizializzazione dello sketch DollaTek verificato, ma orientamento R0.
  statusOLED.begin();
  statusOLED.enableUTF8Print();
  statusOLED.setPowerSave(0);
  statusOLED.setContrast(50);
  statusOLEDReady=true;

  statusOLED.clearBuffer();
  statusOLED.setFont(u8g2_font_7x14B_tf);
  statusOLED.setCursor(0,14);
  statusOLED.print("iDotMatrix B63");
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

  // Scaduto l'avviso: forza un redraw immediato della dashboard normale.
  bool alertExpired = false;
  if (unknownCommandActive && (uint32_t)(now - unknownCommandAt) >= OLED_UNKNOWN_ALERT_MS) {
    unknownCommandActive = false;
    alertExpired = true;
  }

  // Eventi che meritano aggiornamento immediato dell'OLED.
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

  // BUILD 62: nessun refresh periodico. Lo SW-I2C blocca il loop durante
  // sendBuffer(); l'OLED viene quindi ridisegnato solo su un vero cambio di stato.
  if (!stateChanged) return;

  // Snapshot dello stato visualizzato: nessun sendBuffer() finche lo stato non cambia.
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

  // Comando non gestito: alert immediato per 8 secondi.
  if (unknownCommandActive) {
    char line[32];
    statusOLED.setFont(u8g2_font_7x14B_tf);
    statusOLED.drawStr(0,13,"CMD SCONOSCIUTO!");
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
  Serial.print("B"); Serial.print(FW_BUILD);
  Serial.print(" rr="); Serial.print((int)esp_reset_reason());
  Serial.print(" h="); Serial.println(ESP.getFreeHeap());
#endif
  pinMode(STATUS_LED_PIN,OUTPUT); setStatusLed(false);
#if OLED_STATUS_ENABLED
  setupStatusOLED();
#endif
#if ALARM_BUZZER_ENABLED && (ALARM_BUZZER_PIN >= 0)
  pinMode(ALARM_BUZZER_PIN,OUTPUT); digitalWrite(ALARM_BUZZER_PIN,BUZZER_ACTIVE_HIGH?LOW:HIGH);
#endif
#if RTC_ENABLED
  Wire.begin();
  rtcReady=rtc.begin();
#if DEBUG_SERIAL
  Serial.print("RTC DS3231: "); Serial.println(rtcReady?"OK":"NOT FOUND");
  if(rtcReady && rtc.lostPower()) Serial.println("RTC: lost power; waiting for BLE time sync");
#endif
#endif
  bool fsOK=LittleFS.begin(true);
#if DEBUG_SERIAL
  Serial.print("LittleFS: "); Serial.println(fsOK ? "OK" : "ERROR");
  Serial.print("ALARM SLOTS: "); Serial.println(ALARM_SLOT_COUNT);
#endif
  loadAlarms();
  loadSchedule();
  FastLED.addLeds<LED_TYPE,MATRIX_PIN,COLOR_ORDER>(leds,PHYSICAL_NUM_LEDS);
  loadBrightnessFromNVS(); FastLED.clear(); FastLED.show(); clearFramebuffer(); fill_solid(gifFrame,NUM_LEDS,CRGB::Black);

  BLEDevice::init(DEVICE_NAME); server=BLEDevice::createServer(); server->setCallbacks(new ServerCallbacks());
  BLEService *fas=server->createService(FA_SERVICE_UUID);
  fa02=fas->createCharacteristic(FA02_UUID,BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_NR); fa02->setCallbacks(new FA02Callbacks());
  fa03=fas->createCharacteristic(FA03_UUID,BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY); fa03->addDescriptor(new BLE2902()); fas->start();
  BLEService *aes=server->createService(AE_SERVICE_UUID);
  ae01=aes->createCharacteristic(AE01_UUID,BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_NR); ae01->setCallbacks(new AE01Callbacks());
  ae02=aes->createCharacteristic(AE02_UUID,BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY); ae02->addDescriptor(new BLE2902()); aes->start();

  BLEAdvertising *adv=BLEDevice::getAdvertising(); BLEAdvertisementData ad;
  ad.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC|ESP_BLE_ADV_FLAG_BREDR_NOT_SPT); ad.setName(DEVICE_NAME); ad.setCompleteServices(BLEUUID(FA_SERVICE_UUID));
  const char mb[]={0x54,0x52,0x00,0x70,(char)IDOTMATRIX_SCREEN_TYPE}; ad.setManufacturerData(String(mb,sizeof(mb))); adv->setAdvertisementData(ad);
  BLEAdvertisementData scan; scan.setCompleteServices(BLEUUID(AE_SERVICE_UUID)); adv->setScanResponseData(scan); adv->start();

#if OTA_ENABLED
  setupOTA();
#endif
  reportHeap("setup complete");
}

// ======================================================
// LOOP
// ======================================================
void loop(){
  uint32_t now=millis();
  flushBrightnessSaveIfNeeded();
#if OLED_STATUS_ENABLED
  updateStatusOLED();
#endif
#if OTA_ENABLED
  if(otaReady)ArduinoOTA.handle(); if(otaRunning){delay(1);return;}
#endif
  if(pendingDeviceInfoPush && (long)(now-deviceInfoPushAt)>=0){pendingDeviceInfoPush=false;if(deviceConnected)sendDeviceInfo();}
  if(pendingSoftReset && (long)(now-softResetAt)>=0){pendingSoftReset=false;resetRuntimeState();}

  updateAlarms();
  updateSchedule();
  updateBuzzer();

  if(displayMode==DISPLAY_EFFECT) updateEffect();
  if(displayMode==DISPLAY_AUDIO){ static uint32_t lastAudioRender=0; if(now-lastAudioRender>=80){ lastAudioRender=now; renderAudio(); } }
  if(displayMode==DISPLAY_GIF) updateGIF();
  if(displayMode==DISPLAY_TEXT) updateTextAnimation();

  if(displayMode==DISPLAY_CLOCK){ static uint32_t last=0; if(now-last>=100){last=now;renderClock();} }

  if(displayMode==DISPLAY_COUNTDOWN){
    static uint32_t last=0; uint32_t remain=countdownRemainingMs;
    if(countdownRunning){ uint32_t e=now-countdownStartMillis; if(e>=countdownRemainingMs){remain=0;countdownRemainingMs=0;countdownRunning=false;if(!countdownFinishSent){countdownFinishSent=true;sendCommandStatus(0x08,0x80,0x03);}} else remain=countdownRemainingMs-e; }
    if(now-last>=200){
      last=now;
      uint32_t remainSec=(remain+999)/1000UL;
      renderMMSS(remainSec, remainSec<=5 ? CRGB::Red : CRGB::White);
    }
  }

  if(displayMode==DISPLAY_STOPWATCH){
    static uint32_t last=0; if(now-last>=200){last=now;uint32_t e=stopwatchElapsedMs;if(stopwatchRunning)e+=now-stopwatchStartMillis;renderMMSS(e/1000UL,CRGB::White);}
  }

#if DEBUG_SERIAL
  static uint32_t lastHeap=0; if(now-lastHeap>=30000){lastHeap=now;reportHeap("periodic");}
#endif
  delay(5);
}
