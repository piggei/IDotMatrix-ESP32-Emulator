# Protocollo BLE iDotMatrix - note di reverse engineering

Questo documento descrive il protocollo osservato tra l'app iDotMatrix e l'emulatore ESP32.

Non disponendo del dispositivo originale, le informazioni derivano dalle catture dell'app e da test differenziali: si modifica una sola impostazione nell'app e si confrontano i pacchetti generati.

## Convenzioni

Stato delle informazioni:

- **CONFERMATO** - comportamento verificato ripetutamente;
- **PARZIALE** - comando funzionante ma con campi o semantica ancora non completamente identificati;
- **SCONOSCIUTO** - pacchetto osservato ma non decodificato.

Tutti i valori esadecimali sono scritti come byte separati. I campi multibyte osservati nel protocollo sono generalmente **little-endian**.

## Trasporto BLE

### Servizi e caratteristiche - CONFERMATO

| Elemento | UUID | Uso osservato |
|---|---|---|
| Servizio FA | `000000fa-0000-1000-8000-00805f9b34fb` | canale principale |
| FA02 | `0000fa02-0000-1000-8000-00805f9b34fb` | App -> dispositivo, write |
| FA03 | `0000fa03-0000-1000-8000-00805f9b34fb` | Dispositivo -> app, notify/read |
| Servizio AE | `0000ae00-0000-1000-8000-00805f9b34fb` | canale secondario |
| AE01 | `0000ae01-0000-1000-8000-00805f9b34fb` | App -> dispositivo |
| AE02 | `0000ae02-0000-1000-8000-00805f9b34fb` | Dispositivo -> app |

L'advertising usato dall'emulatore espone il servizio FA e manufacturer data osservati:

```text
54 52 00 70 01
```

### Lunghezza pacchetto

Nei pacchetti FA02 il primo `uint16` e' normalmente la lunghezza totale del pacchetto, little-endian. Esempio:

```text
05 00 09 80 01
```

`05 00` = 5 byte totali.

Il callback BLE puo' ricevere un pacchetto logico in piu' write; il firmware accumula i dati fino alla lunghezza dichiarata.

## Risposte / ACK

### ACK standard - CONFERMATO

La forma piu' comune e':

```text
05 00 CMD SUB STATUS
```

Esempio:

```text
05 00 04 80 01
```

`STATUS=01` viene usato come normale conferma/accettazione per molti comandi.

### Stato 03 - CONFERMATO, semantica generale PARZIALE

`03` viene usato in almeno due casi importanti:

1. completamento di un trasferimento bulk;
2. ACK di una singola attivita' Schedule completa.

Per le Schedule la distinzione e' fondamentale:

```text
07 80 -> ACK 01
05 80 -> ACK 03
```

Rispondere `01` a una attivita' `05 80` fa segnalare errore all'app e impedisce l'invio delle attivita' successive. Con `03` l'app continua correttamente.

La semantica universale di `01/02/03` non e' ancora considerata completamente decodificata.

---

# Comandi generali

## Device info - CONFERMATO

### Query

```text
04 00 01 80
```

### Risposta osservata/emulata

```text
09 00 01 80 04 0E 01 01 00
```

La risposta e' sufficiente per far riconoscere all'app il dispositivo come matrice 16x16.

## Sincronizzazione data/ora - CONFERMATO

Pacchetto di 11 byte:

```text
0B 00 01 80 YY MM DD ? HH MI SS
```

Campi usati dal firmware:

| Offset | Campo |
|---:|---|
| 4 | anno come `2000 + YY` |
| 5 | mese |
| 6 | giorno |
| 7 | campo non ancora usato/identificato con certezza |
| 8 | ora |
| 9 | minuto |
| 10 | secondo |

Esempio catturato:

```text
0B 00 01 80 1A 08 1E 07 14 0B 35
```

Il firmware usa il sync come base del clock software. Se `RTC_ENABLED=1`, lo stesso comando puo' sincronizzare anche il DS3231.

ACK:

```text
05 00 01 80 01
```

## Accensione/spegnimento matrice - CONFERMATO

```text
05 00 07 01 STATE
```

`STATE`:

- `00` = off;
- valore non zero = on.

ACK standard `01`.

## Rotazione 180 gradi - CONFERMATO

```text
05 00 06 80 STATE
```

`00` disabilita, valore non zero abilita la rotazione.

## Luminosita' - CONFERMATO

```text
05 00 04 80 PERCENT
```

`PERCENT` e' 0..100. Nel firmware viene scalato su `MAX_LED_BRIGHTNESS` e salvato in NVS con scrittura ritardata.

## Risparmio energetico - PARZIALE

```text
0A 00 02 80 ENABLE SH SM EH EM REDUCTION
```

Interpretazione implementata:

| Campo | Significato |
|---|---|
| ENABLE | abilitazione |
| SH:SM | inizio fascia |
| EH:EM | fine fascia |
| REDUCTION | percentuale di riduzione |

La logica supporta anche fasce che attraversano la mezzanotte.

## Soft reset runtime - PARZIALE

```text
04 00 03 80
```

Il firmware risponde con ACK e resetta lo stato grafico runtime poco dopo. La corrispondenza esatta con il comportamento dell'hardware originale non e' verificabile.

---

# Contenuti grafici

## Colore pieno - CONFERMATO

```text
07 00 02 02 R G B
```

I tre canali sono RGB 8 bit.

## Modalita' graffiti / DIY - CONFERMATO

Entrata/uscita:

```text
05 00 04 01 STATE
```

Aggiornamento pixel:

```text
LENlo LENhi 05 01 ? R G B X0 Y0 X1 Y1 ...
```

Nel firmware:

- RGB = offset 5..7;
- da offset 8 seguono coppie `(x,y)`;
- coordinate valide: 0..15.

Il byte a offset 4 non e' ancora documentato semanticamente.

## Trasferimenti bulk - CONFERMATO per GIF/RAW/TEXT

Header comune implementato, 16 byte:

| Offset | Size | Campo |
|---:|---:|---|
| 0 | 2 | lunghezza pacchetto |
| 2 | 1 | tipo |
| 3 | 1 | `00` |
| 4 | 1 | campo non identificato nel parser attuale |
| 5 | 4 | dimensione totale payload LE |
| 9 | 4 | CRC32 payload LE |
| 13..15 | 3 | campi header non ancora documentati |
| 16.. | - | chunk payload |

Tipi implementati:

| Tipo | Contenuto |
|---:|---|
| `01` | GIF |
| `02` | RAW RGB 16x16 quando size=768 |
| `03` | TEXT |

Durante un trasferimento incompleto:

```text
05 00 TYPE 00 01
```

Al completamento:

```text
05 00 TYPE 00 03
```

Il CRC32 viene verificato sull'intero payload.

### RAW RGB 16x16 - CONFERMATO

Dimensione:

```text
16 * 16 * 3 = 768 byte
```

Ordine pixel lineare RGB. Il firmware converte poi le coordinate logiche nella disposizione serpentina fisica della matrice.

### GIF - CONFERMATO

Il payload e' un normale file GIF (`GIF87a`/`GIF89a`) 16x16. Viene conservato in RAM e decodificato con AnimatedGIF, rispettando delay, palette e trasparenza.

### TEXT - CONFERMATO per i campi usati

Payload globale:

| Offset | Campo |
|---:|---|
| 0 | numero glifi |
| 1..3 | campi non ancora documentati |
| 4 | effetto/movimento |
| 5 | velocita' |
| 6 | modalita' colore |
| 7 | R testo |
| 8 | G testo |
| 9 | B testo |
| 10 | modalita' sfondo |
| 11 | R sfondo |
| 12 | G sfondo |
| 13 | B sfondo |

Seguono record da 20 byte per ogni glifo:

```text
7 byte META + 13 byte BITMAP
```

Ogni bitmap rappresenta 13 righe x 8 colonne. Nel formato osservato **bit 0 e' il pixel sinistro**. Questa inversione e' stata verificata con testi non simmetrici.

Esempio osservato per `IW`:

```text
GLYPH 0
META   : 02 FF FF FF 00 00 00
BITMAP : 3E 08 08 08 08 08 08 08 08 08 3E 00 00

GLYPH 1
META   : 02 FF FF FF 00 00 00
BITMAP : 6B 2A 2A 2A 2A 2A 36 14 14 14 14 00 00
```

La semantica completa dei 7 byte META non e' ancora decodificata.

---

# Effetti luminosi

## Comando effetti - CONFERMATO

```text
LENlo LENhi 03 02 EFFECT SPEED COUNT [R G B]...
```

| Campo | Significato |
|---|---|
| EFFECT | indice effetto |
| SPEED | velocita' |
| COUNT | numero colori |
| RGB | colori coinvolti |

I canali colore osservati in questo comando sono su scala circa **0..127**; il firmware li espande a 0..255.

Sono stati riprodotti 7 effetti grafici osservati nell'app. La resa e' stata rifinita confrontandola visivamente con un video dell'app; non tutti gli algoritmi possono essere considerati una copia matematica dell'originale.

---

# Orologio

## Selezione stile - CONFERMATO

```text
08 00 06 01 FLAGS R G B
```

Interpretazione implementata:

```text
style     = FLAGS & 0x3F
24h       = FLAGS & 0x40
showDate  = FLAGS & 0x80
```

`R G B` e' il colore scelto dall'app; alcuni stili usano anche colori grafici propri osservati nel riferimento visivo.

Sono implementati gli 8 stili studiati durante il reverse engineering.

---

# Countdown

## Comando - CONFERMATO, compatibilita' app PARZIALE

```text
07 00 08 80 MODE MIN SEC
```

`MIN` e `SEC` vengono convertiti in millisecondi.

Modalita' osservate:

| MODE | Azione |
|---:|---|
| `00` | reset |
| `01` | start con valore MIN:SEC |
| `02` | pausa |
| `03` | resume |

Per start/pause/resume/reset il firmware restituisce attualmente ACK standard `01`.

Alla conclusione naturale il firmware invia spontaneamente:

```text
05 00 08 80 03
```

La logica locale del countdown funziona, ma la compatibilita' dell'interfaccia app non e' ancora considerata chiusa.

---

# Cronometro / Stopwatch

## Comando - CONFERMATO, risposta originale NON NOTA

```text
05 00 09 80 MODE
```

Modalita':

| MODE | Azione |
|---:|---|
| `00` | reset |
| `01` | start da zero |
| `02` | pausa |
| `03` | resume |

La state machine locale e' stata verificata: in una prova la pausa dopo circa 6 secondi riportava internamente 6045 ms, il resume continuava da quel valore e il reset tornava a zero.

Lo sniffer ha dimostrato che l'app **non effettua polling periodico** del tempo. Tra START e PAUSE non vengono inviati altri comandi.

Il firmware risponde attualmente:

```text
05 00 09 80 01
```

ma l'interfaccia app non si comporta ancora come atteso. Sono state provate anche varianti `01/03` senza risultato. Il comportamento esatto della risposta del dispositivo originale resta una delle principali domande aperte.

---

# Scoreboard

## Comando - CONFERMATO

```text
08 00 0A 80 A_lo A_hi B_lo B_hi
```

I due punteggi sono `uint16` little-endian.

---

# Audio / Rhythm

L'app non usa il microfono del dispositivo emulato: invia al display dati derivati dall'audio del telefono.

Sono state osservate **10 modalita'**, divise in 5 LEVEL e 5 FFT.

## LEVEL - CONFERMATO

```text
06 00 00 02 LEVEL MODE
```

- `MODE`: 1..5;
- `LEVEL`: livello istantaneo, limitato a 0..12 nel renderer.

ACK:

```text
05 00 00 02 01
```

Sequenza grafica studiata:

1. omino/breakdance;
2. cuore;
3. pseudo-spettrogramma con cornice puntinata;
4. faccia;
5. faccia/labbra animate.

## FFT - CONFERMATO per il frame usato

Frame logico da almeno 21 byte:

```text
21 00 01 02 MODE ...
```

- `MODE`: 0..4;
- il firmware usa 8 bande da offset 5;
- valori limitati a 0..12.

Durante le catture un write BLE da 33 byte poteva contenere un frame completo da 21 byte piu' l'inizio del successivo; il parser considera i primi 21 byte come frame logico.

ACK:

```text
05 00 01 02 01
```

Modalita' grafiche:

1. barre verticali simmetriche dalla linea centrale;
2. simile alla precedente, ma colore associato alle righe;
3. cuore arcobaleno a pieno schermo che si comprime/espande con le barre;
4. spettro dalla linea centrale verticale;
5. barre dall'alto e dal basso verso il centro.

---

# Sveglie

## Comando - CONFERMATO per la struttura implementata

I pacchetti sveglia usano:

```text
CMD=00 SUB=80
```

Il firmware prevede 10 slot (`0..9`).

### Pacchetto completo

Header da 24 byte:

| Offset | Size | Campo |
|---:|---:|---|
| 0 | 2 | lunghezza totale LE |
| 2 | 1 | `00` |
| 3 | 1 | `80` |
| 4 | 1 | slot |
| 5 | 1 | flags |
| 6 | 1 | ora |
| 7 | 1 | minuto |
| 8 | 1 | durata in secondi |
| 9 | 1 | reserved1 |
| 10 | 1 | contentType |
| 11 | 1 | buzzer |
| 12 | 1 | reserved2 |
| 13 | 4 | mediaSize LE |
| 17 | 4 | mediaCRC32 LE |
| 21 | 2 | reserved3 LE |
| 23 | 1 | mediaId |
| 24.. | - | media |

Tipi media confermati nel firmware:

- `01` = GIF;
- `02` = RAW RGB.

Durante le prove sono comparsi anche contenuti testuali in pacchetti sveglia; la semantica completa di tutti i valori `contentType` va ancora documentata con catture dedicate.

### Flags giorni - CONFERMATO

La convenzione usata anche dalle Schedule e':

```text
bit 0 = enabled
bit 1 = lunedi
bit 2 = martedi
bit 3 = mercoledi
bit 4 = giovedi
bit 5 = venerdi
bit 6 = sabato
bit 7 = domenica
```

Per una sveglia one-shot i bit giorno possono essere zero; dopo l'esecuzione il firmware disabilita il bit `enabled`.

### Pacchetto corto

Se il pacchetto e' piu' corto dell'header completo, il firmware lo tratta come aggiornamento/disabilitazione dei metadati senza riscrivere il media.

### ACK

```text
05 00 00 80 01
```

Il buzzer e' previsto dal protocollo e dal firmware ma non e' presente sull'hardware di sviluppo attuale.

---

# Programmi / Schedule

Questa e' una delle parti del protocollo meglio verificate, grazie a programmi con 1, 3 e almeno 12 attivita'.

## Stato globale programma - CONFERMATO

```text
05 00 07 80 FLAGS
```

Interpretazione verificata:

```text
bit 0 = programma abilitato
bit 1 = suono abilitato
```

Esempi osservati:

- `00` = disabled, sound off;
- `01` = enabled, sound off;
- `03` = enabled, sound on.

ACK obbligatorio:

```text
05 00 07 80 01
```

Quando un programma viene attivato, l'app invia le attivita' una alla volta, attendendo l'ACK di ciascuna.

## Attivita' - CONFERMATO

Formato:

| Offset | Size | Campo |
|---:|---:|---|
| 0 | 2 | lunghezza totale LE |
| 2 | 1 | `05` |
| 3 | 1 | `80` |
| 4 | 1 | indice attivita' |
| 5 | 1 | flags |
| 6 | 1 | ora inizio |
| 7 | 1 | minuto inizio |
| 8 | 1 | ora fine |
| 9 | 1 | minuto fine |
| 10 | 2 | contentType LE |
| 12 | 4 | payloadSize LE |
| 16 | 4 | CRC32 LE |
| 20 | 2 | reserved LE |
| 22 | 1 | mediaId |
| 23.. | - | payload |

Tipi:

```text
01 = GIF
02 = IMAGE / PNG 16x16
03 = TEXT
```

### Flags attivita' - CONFERMATO

```text
bit 0 = enabled
bit 1 = lunedi
bit 2 = martedi
bit 3 = mercoledi
bit 4 = giovedi
bit 5 = venerdi
bit 6 = sabato
bit 7 = domenica
```

Esempi verificati:

```text
03 = enabled + lunedi
4B = enabled + lunedi + mercoledi + sabato
D5 = enabled + martedi + giovedi + sabato + domenica
C1 = enabled + sabato + domenica
A5 = enabled + martedi + venerdi + domenica
```

### ACK attivita' - CONFERMATO E CRITICO

In caso di successo:

```text
05 00 05 80 03
```

Questo e' stato verificato sperimentalmente. Rispondendo:

```text
05 00 05 80 01
```

l'app segnala errore anche per un programma con una sola attivita' e non prosegue con le successive. Con `03`, un programma di 12 attivita' viene trasferito completamente.

Il firmware usa `02` come risposta in caso di validazione fallita; il significato originale di questo status non e' ancora confermato da un dispositivo reale.

### Commit

Non e' stato osservato un comando esplicito di fine lista. Il firmware quindi usa uno staging temporaneo e considera concluso l'upload dopo circa 900 ms senza nuove attivita'. A quel punto sostituisce atomicamente la Schedule precedente.

Questa e' una scelta dell'emulatore, non un campo confermato del protocollo originale.

### PNG

Le immagini Schedule osservate sono PNG 16x16, 8 bit, non interlacciate. Il decoder implementato supporta RGB/RGBA e i filtri PNG standard. L'inflate usa direttamente `tinfl_decompressor` con stato allocato su heap: l'uso del wrapper `tinfl_decompress_mem_to_mem()` causava stack overflow del `loopTask` sull'ESP32 usato.

---

# Campi e comportamenti ancora aperti

1. risposta esatta del dispositivo originale al cronometro `09 80`;
2. semantica generale completa degli status ACK `01`, `02`, `03`;
3. significato di alcuni byte riservati negli header Bulk, Alarm e Schedule;
4. significato completo dei 7 byte META di ogni glifo TEXT;
5. eventuali comandi dell'app non ancora visitati;
6. corrispondenza matematica esatta di alcuni effetti grafici/audio rispetto al firmware originale.

La BUILD 60 registra ogni comando non riconosciuto e, se l'OLED e' abilitato, ne mostra temporaneamente lunghezza e primi byte direttamente sul display diagnostico.
