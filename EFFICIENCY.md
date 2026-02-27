# ⚡ Stromeffizienz-Architektur – Two-Stage Bird Detection
### SenseBox ESP32-S3 | Wächterstrategie für kontinuierliches Monitoring

---

## 🎯 Grundprinzip

Statt das 10-Arten-Modell 24h durchlaufen zu lassen, wird eine zweistufige Pipeline verwendet. Stage 1 ist ein winziges Binary-Modell das als "Torwächter" fungiert – nur bei einem positiven Treffer wird Stage 2 (das eigentliche Klassifikationsmodell) geweckt.

```
Mikrofon läuft immer (ultra-low power)
        ↓
  Stage 1: "Ist da überhaupt ein Vogel?"   ← klein, schnell, günstig
        ↓  (nur bei Score > 0.55)
  Stage 2: "Welche Art ist es?"            ← dein 10-Arten-CNN
        ↓
  Ergebnis loggen / senden → zurück in Sleep
```

---

## 🔋 Stromverbrauch

| Zustand | Strom |
|---|---|
| Deep Sleep | ~10–20 µA |
| Frequenzcheck aktiv | ~10–20 mA |
| Stage 1 Inferenz | ~50–80 mA für ~5 ms |
| Stage 2 Inferenz | ~80–100 mA für ~100 ms |
| WiFi-Transmission | ~150–200 mA für ~100 ms |

Mit dieser Pipeline läuft das Board bei einer 3.000 mAh Batterie **tagelang** statt Stunden.

---

## 🧠 Stage 1 – Binary Bird/No-Bird Modell

Ein zweites, winziges TFLite-Modell mit nur zwei Klassen: **Vogel** vs. **Kein Vogel**.

### Eigenschaften
- Modellgröße: **< 20 KB**
- Inferenzzeit: **~5 ms**
- Trainiert mit denselben Daten wie Stage 2 → kein extra Aufwand

### Confidence Threshold bewusst niedrig halten
```
Bird-Score > 0.55  →  Stage 2 triggern   (lieber false positive als miss)
Bird-Score ≤ 0.55  →  zurück in Sleep
```
False Positives (Stage 2 unnötig getriggert) kosten nur minimal Strom. Ein verpasster Vogelruf ist verloren.

---

## 📦 Trainingsdaten für Stage 1

Stage 1 wird **mit denselben xeno-canto-Daten trainiert** die sowieso für Stage 2 gesammelt werden – alle 10 Arten einfach zu einer "Vogel"-Klasse zusammenwerfen. Keine Doppelarbeit.

### Empfohlene Klassenverteilung

| Klasse | Clips | Quellen |
|---|---|---|
| Vogel (alle Arten gemischt) | ~500–800 | xeno-canto – aus Stage-2-Daten recyceln |
| Kein Vogel | ~300–400 | Freesound + selbst aufgenommen |

**Warum 60/40 statt 50/50?** In der echten Welt gibt es deutlich mehr Nicht-Vogel-Momente. Das Modell soll konservativ triggern – lieber Stage 2 einmal zu oft anwerfen als einen Ruf verpassen.

### Was in die "Kein Vogel"-Klasse muss
Nicht nur Studio-Rauschen, sondern echte Outdoor-Geräusche:
- Wind, Regen, Blätterrauschen → Freesound
- Menschenstimmen, Schritte, Autos → Freesound
- Andere Tiere: Hunde, Insekten, Frösche → Freesound / xeno-canto
- **Stille / Hintergrundrauschen** → selbst mit dem INMP441 aufnehmen, da jedes Mikrofon anders klingt

---

## 🔄 Ringpuffer-Architektur

Stage 1 und Stage 2 laufen **sequentiell**, nicht parallel – beide brauchen denselben Audio-Input, Parallelität bringt keinen Vorteil. Damit trotzdem kein Rufanfang verloren geht, wird ein Ringpuffer verwendet.

### Funktionsweise

```
Mikrofon schreibt kontinuierlich in Ringpuffer (3s)
              ↓
  Stage 1 analysiert letztes 1s-Fenster (alle 500ms)
              ↓  Vogel erkannt!
  Stage 2 bekommt alle 3s aus dem Puffer
              ↓
  3x 1s-Fenster → Mehrheitsentscheid → stabileres Ergebnis
```

### Speicherbedarf Ringpuffer
```
3 Sekunden × 16.000 Hz × 2 Bytes (int16) = 96 KB SRAM
```
Passt problemlos ins SRAM des ESP32-S3 ohne PSRAM zu benötigen.

---

## 🏗️ Finale Systemarchitektur

```
┌─────────────────────────────────────────────────────┐
│                   Deep Sleep (~10µA)                │
└──────────────────────┬──────────────────────────────┘
                       │ Wake-on-Sound (Hardware VAD)
┌──────────────────────▼──────────────────────────────┐
│            Ringpuffer befüllen (3s, 96KB)           │
│         Stage 1 prüft alle 500ms ein 1s-Fenster     │
└──────────────────────┬──────────────────────────────┘
                       │ Score > 0.55
┌──────────────────────▼──────────────────────────────┐
│     Stage 2: 3x 1s-Fenster aus Ringpuffer           │
│     Mehrheitsentscheid → Art bestimmen              │
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│        Ergebnis loggen / per MQTT senden            │
│                → zurück in Deep Sleep               │
└─────────────────────────────────────────────────────┘
```

---

## 🚫 Bewusst ausgeschlossen: Gleichzeitiger Betrieb Stage 1 + 2

Während Stage 2 eine Erkennung verarbeitet (~100 ms) könnte theoretisch ein neuer Vogel beginnen zu rufen. Dieses Szenario wird **bewusst ignoriert**, weil:

- Vogelrufe dauern typischerweise 2–5 Sekunden – die 100 ms Überlappung ist vernachlässigbar
- Das System ist ein **Feldmonitor, kein Echtzeit-Alarmsystem** – ein verpasster Ruf von zehn ist akzeptabel
- Die Alternative (Queuing, Interrupt-Handler, Doppelpuffer) würde die Komplexität massiv erhöhen für minimalen Gewinn

---

## 📋 Zusammenfassung der Design-Entscheidungen

| Entscheidung | Begründung |
|---|---|
| Stage 1 Threshold 0.55 statt 0.80 | Lieber false positive als verpasster Ruf |
| 60/40 Klassenverteilung | Spiegelt reale Außenbedingungen wider |
| Ringpuffer 3s statt 1s | Rufanfang geht nicht verloren |
| Mehrheitsentscheid 3 Fenster | Stabilere Ergebnisse als einzelner Clip |
| Keine Parallelität Stage 1+2 | Unnötige Komplexität für minimalen Gewinn |
| "Kein Vogel"-Daten selbst aufnehmen | Mikrofon-spezifisches Rauschen wichtig |

---

*Ergänzung zur Hauptdoku: `birdnet_sensebox_projekt.md`*