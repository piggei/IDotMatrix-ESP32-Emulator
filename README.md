# iDotMatrix ESP32 Emulator

Firmware sperimentale per ESP32 che emula un display **iDotMatrix 16x16** e comunica direttamente con l'app iDotMatrix tramite Bluetooth Low Energy.

Il progetto nasce da reverse engineering del traffico BLE generato dall'app, senza disporre del dispositivo originale. L'obiettivo e' riprodurre il maggior numero possibile di funzioni dell'app su una matrice WS2812B 16x16 pilotata da ESP32, documentando contemporaneamente il protocollo osservato.

> Stato attuale: **BUILD 60**, baseline stabile e consolidata.

## Hardware usato

Configurazione di sviluppo attuale:

- DollaTek ESP32 OLED 0,96" con ESP32 e CP2102;
- matrice 16x16 WS2812B, 256 LED;
- GPIO matrice: **17**;
- LED di stato: **GPIO 25**;
- OLED SSD1306 integrato 128x64:
  - SCL: GPIO 15
  - SDA: GPIO 4
  - RESET: GPIO 16
  - gestito tramite U8g2 software-I2C;
- buzzer: predisposto nel firmware ma non installato;
- RTC DS3231: supporto opzionale predisposto ma non installato.

La luminosita' massima FastLED e' limitata da `MAX_LED_BRIGHTNESS` (attualmente 50/255) per contenere l'assorbimento della matrice.

## Funzioni implementate

Il firmware supporta attualmente:

- advertising BLE compatibile con l'app;
- servizi e caratteristiche FA/AE osservati;
- identificazione dispositivo 16x16;
- sincronizzazione data/ora dall'app;
- accensione e spegnimento matrice;
- rotazione 180 gradi;
- luminosita' con persistenza NVS;
- risparmio energetico programmato;
- colore pieno;
- modalita' disegno/graffiti;
- immagini RAW RGB 16x16;
- GIF animate;
- testo con glifi, colori, sfondo ed effetti;
- effetti luminosi;
- orologio e relativi stili grafici;
- scoreboard;
- 10 effetti audio: 5 LEVEL e 5 FFT;
- sveglie persistenti, con media associato;
- Programmi/Schedule persistenti con GIF, testo e PNG;
- decoder PNG 16x16;
- OLED diagnostico opzionale;
- visualizzazione sull'OLED dei comandi BLE non ancora riconosciuti.

Countdown e cronometro sono implementati localmente ma la compatibilita' dell'interfaccia dell'app non e' ancora completa. Vedi [TODO.md](TODO.md).

## Dipendenze Arduino

Il sorgente utilizza:

- ESP32 Arduino Core;
- FastLED;
- AnimatedGIF;
- U8g2, se `OLED_STATUS_ENABLED` e' attivo;
- LittleFS e Preferences incluse nel core ESP32;
- RTClib solo se `RTC_ENABLED` viene portato a 1.

Il decoder PNG usa `miniz` disponibile nel core/SDK ESP32 e non richiede una libreria PNG esterna.

## Partizionamento flash

Durante lo sviluppo il firmware ha superato la dimensione disponibile nella configurazione con OTA. Poiche' OTA non viene utilizzato, la scheda viene compilata con una partizione che assegna circa **2 MB all'applicazione senza OTA**.

Se in futuro la dimensione dovesse crescere ulteriormente, e' possibile usare una configurazione con circa 3 MB per lo sketch, pur mantenendo spazio sufficiente per LittleFS.

## Configurazione principale

I parametri piu' importanti sono raccolti all'inizio di `src/IDotMatrix.ino`, tra cui:

```cpp
#define FW_BUILD               60
#define DEBUG_SERIAL           1
#define MATRIX_PIN             17
#define STATUS_LED_PIN         25
#define OLED_STATUS_ENABLED    1
#define RTC_ENABLED            0
#define MAX_LED_BRIGHTNESS     50
#define ALARM_SLOT_COUNT       10
#define SCHEDULE_MAX_ACTIVITIES 32
```

Il numero di build viene incrementato a ogni versione distribuita ed e' mostrato anche sull'OLED diagnostico.

## OLED diagnostico

L'OLED integrato puo' essere escluso completamente a compile-time:

```cpp
#define OLED_STATUS_ENABLED 0
```

Quando attivo mostra, tra le altre cose:

- build firmware;
- stato BLE;
- stato matrice ON/OFF;
- modalita' grafica corrente;
- ora sincronizzata;
- luminosita';
- heap e stack disponibili;
- stato Schedule;
- stato countdown/cronometro;
- numero di comandi sconosciuti.

Quando arriva un comando non riconosciuto, per alcuni secondi viene mostrato un avviso con lunghezza e primi byte del pacchetto. Questo e' particolarmente utile quando non e' possibile tenere aperto il monitor seriale.

## Persistenza

Sono usati due meccanismi:

- **NVS / Preferences** per configurazioni e metadati;
- **LittleFS** per media di sveglie e Schedule.

La luminosita' impostata dall'app viene salvata in NVS. Sveglie e Programmi vengono ricostruiti dopo un reboot; per essere eseguiti correttamente serve pero' un riferimento temporale valido. Senza RTC esterno l'ora viene acquisita tramite sincronizzazione BLE dall'app.

## Protocollo BLE

Il protocollo osservato e' descritto in dettaglio in [PROTOCOL.md](PROTOCOL.md).

La documentazione distingue intenzionalmente tra:

- **CONFERMATO**: verificato sperimentalmente;
- **PARZIALE**: implementato ma con campi o semantica non completamente noti;
- **SCONOSCIUTO**: osservato ma non ancora decodificato.

## Stato e roadmap

La lista aggiornata delle funzioni completate, dei problemi noti e delle domande aperte e' in [TODO.md](TODO.md).

## Struttura repository

```text
idotmatrix-esp32/
|-- src/
|   `-- IDotMatrix.ino
|-- docs/
|   `-- captures/
|       `-- README.md
|-- README.md
|-- PROTOCOL.md
|-- TODO.md
`-- .gitignore
```

La cartella `docs/captures/` e' destinata alle catture BLE significative usate come evidenza per il reverse engineering.

## Nota sul reverse engineering

Questo e' un progetto indipendente, costruito osservando il comportamento dell'app e i pacchetti BLE da essa trasmessi. Le descrizioni del protocollo rappresentano quanto verificato sperimentalmente durante lo sviluppo e possono contenere campi ancora non identificati.

## Documentazione di sviluppo

- [`PROTOCOL.md`](PROTOCOL.md) - reverse engineering dettagliato del protocollo BLE.
- [`TODO.md`](TODO.md) - stato delle funzionalita', problemi aperti e sviluppi futuri.
- [`HISTORY.md`](HISTORY.md) - cronologia delle build e delle principali modifiche.
- [`docs/captures/`](docs/captures/) - catture sperimentali significative usate per il reverse engineering.

### Build corrente

La versione sorgente inclusa in questo archivio e' **BUILD 62**, con OLED diagnostico completamente event-driven. La B62 e' stata verificata su hardware senza i rallentamenti della matrice osservati nelle B60/B61.
