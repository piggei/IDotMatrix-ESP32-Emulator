# Catture BLE

Questa cartella e' destinata alle catture minimali usate per il reverse engineering del protocollo.

Formato consigliato per ogni file:

```text
# Scopo della prova
# Versione app / data, se rilevante
# Sequenza di azioni eseguite

RX ...
TX ...
```

Esempi di nomi:

- `stopwatch-start-pause-resume-reset.txt`
- `countdown-10s.txt`
- `schedule-three-activities.txt`
- `alarm-gif.txt`
- `audio-fft-modes.txt`

Evitare log enormi non annotati: una cattura piccola in cui cambia una sola variabile e' molto piu' utile per il reverse engineering.
