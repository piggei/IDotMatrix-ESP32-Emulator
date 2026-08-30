# HISTORY

Cronologia dello sviluppo dell'emulatore ESP32 per iDotMatrix.

> Le build iniziali sono state sviluppate in modo fortemente iterativo durante il reverse engineering e non per tutte e' disponibile una cronologia puntuale affidabile. Dove il numero esatto non e' documentato, questo file descrive il periodo di sviluppo senza attribuire modifiche a una build arbitraria.

## BUILD 62 - OLED pure event-driven

### Fixed
- Eliminati gli ultimi scatti delle animazioni della matrice causati dai trasferimenti periodici dell'OLED tramite software I2C.
- Rimosso qualsiasi refresh temporizzato dell'OLED durante il normale funzionamento.

### Changed
- OLED aggiornato esclusivamente in risposta a eventi significativi: BLE, power, modalita', luminosita', Schedule/Alarm, timer e comandi sconosciuti.
- Stopwatch e countdown sull'OLED mostrano lo stato all'evento, senza ridisegno continuo del tempo trascorso.

### Status
- Verificata su hardware: animazioni della matrice nuovamente fluide con OLED abilitato.

## BUILD 61 - OLED event-driven + refresh lento

### Fixed
- Prima correzione della regressione prestazionale introdotta dalla dashboard OLED della B60.

### Changed
- Refresh normale OLED ridotto a circa 2 secondi.
- Aggiornamenti immediati sugli eventi principali.
- Countdown/stopwatch limitati a circa un aggiornamento al secondo.
- Rimossa l'indicazione uptime dalla dashboard.

### Known issues
- Il refresh periodico completo del framebuffer SSD1306 tramite U8g2 software I2C produceva ancora scatti visibili sulla matrice. Risolto in B62.

## BUILD 60 - Baseline consolidata

### Added
- Prima baseline consolidata destinata al repository.
- Monitor dei comandi FA02 non gestiti direttamente sull'OLED.
- Alert OLED con lunghezza, contatore e primi 12 byte RAW del comando sconosciuto.
- Contatore `UNK` dei comandi non riconosciuti.

### Changed
- Configurazione hardware consolidata sulla DollaTek ESP32 OLED.
- Define configurabili raccolti e ripuliti.
- Diagnostica PNG sperimentale ridotta dopo la stabilizzazione del decoder.

### Known issues
- Dashboard OLED ridisegnata ogni 250 ms con `F_SW_I2C`: forte rallentamento delle animazioni. Mitigato in B61 e risolto in B62.

## BUILD 59 - Dashboard OLED live

### Added
- Dashboard OLED con BLE, stato schermo, modalita', ora, luminosita', Schedule, heap e stack.
- Rotazione OLED corretta per l'orientamento fisico della scheda.

### Changed
- Font OLED reso piu' leggibile.

## BUILD 58 - Test OLED esatto

### Fixed
- Inizializzazione OLED allineata allo sketch U8g2 gia' verificato sulla scheda reale.
- Sequenza `begin`, power-save e contrasto verificata con splash iniziale.

## BUILD 57 - Migrazione OLED a U8g2

### Changed
- Rimossi Adafruit GFX/SSD1306 per la diagnostica OLED.
- Utilizzata U8g2 in software I2C con SCL=15, SDA=4, RESET=16.

## BUILD 56 - Fix compilazione OLED

### Fixed
- Risolto il conflitto del preprocessore Arduino con `DisplayMode` nella funzione di diagnostica OLED.

## BUILD 55 - Prima diagnostica OLED

### Added
- Primo supporto al display OLED onboard, inizialmente tramite librerie Adafruit.
- `OLED_STATUS_ENABLED` per escludere completamente il codice a compile-time.

## BUILD 54 - Timer protocol sniffer

### Diagnostics
- Logging RAW dei frammenti FA02 e del pacchetto FA02 ricostruito.
- Diagnostica mirata di stopwatch e countdown.

### Protocol
- Confermato che l'app non effettua polling periodico dei timer.
- Confermati i quattro stati stopwatch: reset/start/pause/resume.
- Confermati i quattro stati countdown e la notifica spontanea di fine countdown.

## BUILD 53 e precedenti - stabilizzazione Schedule/PNG

### Fixed
- Stabilizzato il caricamento e la visualizzazione delle immagini PNG delle attivita' Schedule.
- Individuato e risolto overflow dello stack durante inflate PNG spostando i buffer pesanti fuori dallo stack del loop task.
- Gestione corretta delle attivita' multiple e ritorno alla modalita' precedente.

### Protocol
- Scoperto che l'ACK delle attivita' Schedule deve terminare con status `0x03`; `0x01` fa interrompere l'invio all'app e genera errore.
- Implementata ricezione sequenziale di programmi con molte attivita'.

## Periodo Alarm / Schedule

### Added
- 10 slot Alarm persistenti in flash/LittleFS.
- Media associati alle sveglie: GIF, immagini e testo secondo il payload ricevuto.
- Giorni della settimana, durata, enable e flag buzzer.
- Programmi/Schedule persistenti con attivita' temporizzate.
- Supporto Schedule per GIF, testo e PNG.
- Fino a 32 attivita' Schedule nel firmware consolidato.

### Protocol
- Reverse engineering dei comandi globali Schedule `07 80` e delle attivita' `05 80`.
- Confermato che l'app prepara/modifica localmente i programmi e trasferisce le attivita' al device quando il programma viene attivato.

## BUILD 32 - Audio protocol diagnostics

### Diagnostics
- Build identificata nei log come `B32-audio-protocol-diagnostics`.
- Logging distinto dei pacchetti LEVEL e FFT.

### Protocol
- Identificate 5 modalita' LEVEL e 5 modalita' FFT usate dall'app.
- Confermata la presenza dei valori di livello/sensibilita' nei pacchetti LEVEL.
- Confermata la struttura a bande dei pacchetti FFT e i relativi ACK.

## Periodo effetti audio

### Added
- Implementazione grafica delle 10 visualizzazioni audio osservate nell'app.
- Cinque effetti LEVEL: omino, cuore, spettro con cornice puntinata e due facce animate.
- Cinque effetti FFT: barre simmetriche e varianti cromatiche, cuore reattivo, spettro orizzontale/verticale.

### Changed
- Corretto mapping degli indici dopo confronto sistematico con il video dell'app.
- Rifinite singolarmente le grafiche osservando il comportamento originale dell'app.

## Periodo GIF / Bulk / Text / Effects

### Added
- Ricostruzione dei pacchetti BLE frammentati in FA02.
- Trasferimenti bulk con dimensione, CRC32 e ACK intermedi/finali.
- Riproduzione GIF 16x16 tramite AnimatedGIF.
- Testo con payload bitmap/glyph ricevuti dall'app.
- Graffiti/DIY e colore pieno.
- Effetti grafici configurabili con velocita' e palette.
- Stili orologio, scoreboard, luminosita', rotazione e risparmio energetico.

### Fixed
- Numerose correzioni alla gestione GIF: trasparenza, disposal, frame timing e restart.
- Gestione memoria progressivamente ridotta per mantenere heap sufficiente con BLE e FastLED.

## Build iniziali

### Added
- Advertising BLE compatibile con l'app iDotMatrix.
- Servizi/caratteristiche FA e AE.
- Device info 16x16.
- Time sync dall'app.
- Controllo ON/OFF della matrice.
- Prima implementazione degli ACK FA03.

### Note
- La numerazione puntuale delle primissime build non e' stata conservata in modo abbastanza affidabile da ricostruire un changelog per ogni singolo numero senza introdurre supposizioni.
