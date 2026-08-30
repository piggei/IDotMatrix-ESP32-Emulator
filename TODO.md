# TODO / stato del progetto

Questo file separa volutamente:

- funzioni completate;
- funzioni parziali;
- hardware opzionale;
- problemi noti;
- domande ancora aperte sul protocollo.

## Completato

- [x] Advertising BLE riconosciuto dall'app.
- [x] Servizi FA e AE e caratteristiche FA02/FA03/AE01/AE02.
- [x] Device info / riconoscimento matrice 16x16.
- [x] Sincronizzazione data e ora dall'app.
- [x] Accensione/spegnimento matrice.
- [x] Rotazione 180 gradi.
- [x] Luminosita' 0..100 con limite hardware configurabile.
- [x] Persistenza luminosita' in NVS.
- [x] Risparmio energetico a fascia oraria.
- [x] Colore pieno.
- [x] Modalita' graffiti/DIY.
- [x] Immagini RAW RGB 16x16.
- [x] GIF animate.
- [x] Testo e glifi con orientamento bitmap corretto.
- [x] Colori ed effetti testo studiati.
- [x] Effetti luminosi studiati e rifiniti visivamente.
- [x] Orologio e stili grafici studiati.
- [x] Scoreboard.
- [x] Audio: 5 modalita' LEVEL.
- [x] Audio: 5 modalita' FFT.
- [x] Sveglie: parsing e 10 slot persistenti.
- [x] Sveglie: durata, giorni, one-shot e media.
- [x] Programmi/Schedule: stato globale ON/OFF e sound flag.
- [x] Programmi/Schedule: handshake corretto `07 80 -> 01`, `05 80 -> 03`.
- [x] Programmi/Schedule: almeno 12 attivita' verificate; limite firmware 32.
- [x] Programmi/Schedule: persistenza NVS + LittleFS.
- [x] Programmi/Schedule: staging e commit della nuova lista.
- [x] Programmi/Schedule: esecuzione automatica per giorno e fascia oraria.
- [x] Programmi/Schedule: TEXT.
- [x] Programmi/Schedule: GIF.
- [x] Programmi/Schedule: PNG 16x16.
- [x] Decoder PNG con inflater su heap, evitando stack overflow.
- [x] OLED diagnostico U8g2 opzionale.
- [x] OLED: build, BLE, screen, modalita', ora, luminosita', heap/stack, Schedule e timer.
- [x] OLED: avviso evidente per comandi non gestiti con dump dei primi byte.
- [x] BUILD 60 definita come baseline consolidata.

## Parzialmente completato

### Countdown

- [x] Decodificati MODE 0/1/2/3.
- [x] Start, pausa, resume, reset locali.
- [x] Rendering del tempo residuo.
- [x] Evento spontaneo `08 80 03` a fine countdown.
- [ ] Verificare perche' l'interfaccia dell'app non replica ancora perfettamente il comportamento atteso.

### Cronometro

- [x] Decodificati MODE 0/1/2/3.
- [x] Start, pausa, resume, reset locali.
- [x] Conteggio interno verificato.
- [x] Confermato tramite sniffer che l'app non effettua polling periodico.
- [x] Provate varianti ACK `01/03` senza risolvere il problema UI.
- [ ] Capire la risposta/stato che l'app si aspetta dal dispositivo originale.

### Testo

- [x] Struttura globale e bitmap glifi.
- [x] Orientamento bit corretto (`bit0 = sinistra`).
- [ ] Decodificare semanticamente tutti i 7 byte `META` del glifo.

### ACK

- [x] ACK standard `01` verificato su molti comandi.
- [x] `03` confermato come necessario per fine bulk e attivita' Schedule.
- [ ] Formalizzare il significato generale di `01`, `02`, `03` per tutti i sottoprotocolli.

## Hardware opzionale

### Buzzer

- [x] Campi buzzer riconosciuti in sveglie/Schedule.
- [x] Logica firmware predisposta.
- [ ] Scegliere GPIO libero.
- [ ] Collegare buzzer o pilotaggio tramite transistor se necessario.
- [ ] Abilitare `ALARM_BUZZER_ENABLED` / `SCHEDULE_BUZZER_ENABLED`.
- [ ] Verificare durata e comportamento sonoro rispetto all'app.

### RTC DS3231

- [x] Supporto software opzionale predisposto.
- [x] Sync BLE -> RTC predisposto.
- [ ] Decidere se montarlo realmente.
- [ ] Verificare convivenza I2C con OLED se si passa a bus condiviso/hardware.

L'RTC non e' necessario per emulare il comportamento osservato finche' la scheda resta alimentata e riceve il time-sync BLE. Serve invece per mantenere un riferimento temporale affidabile dopo un power-cycle senza riconnettere l'app.

## Problemi noti

- Il cronometro funziona sulla matrice ma la UI dell'app non e' ancora completamente soddisfatta.
- Il countdown e' funzionale ma va considerato ancora parziale lato compatibilita' app.
- La matrice WS2812B puo' causare brownout se alimentata dalla stessa sorgente della scheda senza corrente sufficiente. Non e' un bug del decoder PNG o del protocollo.
- Alcuni effetti sono repliche visive ottenute da video e test, non algoritmi estratti dal firmware originale.
- Non disponiamo del dispositivo iDotMatrix originale: non e' possibile confrontare direttamente le risposte BLE del firmware originale.

## Domande aperte sul protocollo

- [ ] Qual e' la risposta esatta del device originale a `09 80` (stopwatch)?
- [ ] `STATUS=02` e' realmente un errore/NACK generale?
- [ ] Qual e' la semantica completa dei campi reserved negli header Alarm/Schedule?
- [ ] Qual e' la semantica dei byte 1..3 del payload TEXT?
- [ ] Qual e' la semantica completa dei 7 byte META per glifo?
- [ ] Esistono funzioni dell'app non ancora visitate che generano nuovi comandi?

## Miglioramenti software futuri

- [ ] Separare gradualmente il monolite `.ino` in moduli `.h/.cpp` dopo aver stabilizzato il protocollo.
- [ ] Aggiungere test automatici per parser di pacchetti noti.
- [ ] Salvare in `docs/captures/` catture BLE minimali e annotate per ogni famiglia di comando.
- [ ] Aggiungere un changelog quando inizieranno release/tag Git formali.
- [ ] Valutare CI di compilazione Arduino/PlatformIO.
- [ ] Tradurre README, PROTOCOL e TODO in inglese dopo la revisione della versione italiana.

## Regola per nuove scoperte

Quando viene osservato un nuovo comando:

1. salvare una cattura minima e riproducibile;
2. variare una sola impostazione dell'app alla volta;
3. annotare byte costanti e byte variabili;
4. implementare il comando solo dopo aver identificato almeno la struttura principale;
5. aggiornare `PROTOCOL.md` indicando chiaramente se il risultato e' CONFERMATO o PARZIALE;
6. aggiornare questo TODO.

## Note dalla BUILD 62

- [x] OLED diagnostico U8g2 funzionante sulla DollaTek (SCL 15, SDA 4, RESET 16).
- [x] Eliminata la regressione prestazionale OLED: dalla B62 il display e' aggiornato esclusivamente su evento.
- [x] Comandi non gestiti segnalati direttamente sull'OLED, utile quando il monitor seriale non e' disponibile.
