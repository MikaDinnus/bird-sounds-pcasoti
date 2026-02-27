# 🐦 Vogelarten-Erkennung mit ESP32-S3 & SenseBox
### Projektzusammenfassung – Edge AI mit TensorFlow Lite Micro

---

## 📌 Projektziel

Eigenständiges, offline-fähiges Vogelarten-Erkennungssystem auf Basis der **SenseBox mit ESP32-S3**. Statt des vollständigen BirdNET-Modells (~50 MB, zu groß für MCU) wird ein eigenes, kompaktes TFLite-Modell für **10 lokale Vogelarten** trainiert und direkt auf dem Gerät ausgeführt.

---

## ⚠️ Warum nicht das originale BirdNET-Modell?

| Problem | Details |
|---|---|
| Modellgröße | BirdNET v2.4 ist mehrere MB groß – braucht PSRAM, Inferenz dauert dann ~60 Sek. statt <100 ms |
| RFFT-Layer | BirdNET nutzt RFFT-Schichten für Mel-Spektrogramm-Berechnung, die von TFLite Micro **nicht unterstützt** werden |
| 6.000+ Klassen | Viel zu groß für Microcontroller-Deployment |

**Lösung:** Eigenes, schlankes CNN-Modell mit 10–11 Klassen via **Edge Impulse** trainieren.

---

## 🔧 Hardware

- **Board:** SenseBox mit ESP32-S3 (N16R8 empfohlen – 16 MB Flash, 8 MB PSRAM)
- **Mikrofon:** I2S-MEMS-Mikrofon, z.B. **INMP441** oder **SPH0645** (~3–5 €)
- **Ziel-Specs nach Quantisierung:**

| Metrik | Zielwert |
|---|---|
| Modellgröße | < 300 KB (ins Flash) |
| Tensor-Arena | < 100 KB (ins SRAM) |
| Inferenzzeit | 50–100 ms |
| Genauigkeit | 75–90 % |

---

## 📦 Schritt 1: Trainingsdaten sammeln

### Quellen
- **[xeno-canto.org](https://xeno-canto.org)** – beste Quelle, riesige freie Datenbank mit Vogelrufen, nach Qualität filterbar
- **[Macaulay Library](https://www.macaulaylibrary.org)** (Cornell) – hochqualitativ
- **[Freesound.org](https://freesound.org)** – für Hintergrundgeräusche (Klasse "Andere")

### Menge
- **Mindestens 50–100 Clips pro Art**, besser 150–200
- Gesamt: ca. **1.000–2.000 Clips** für 10 Arten
- Jeder Clip: **1–3 Sekunden** (Edge Impulse schneidet automatisch in 1s-Fenster)

### ⚡ Wichtig: 11. Klasse "Andere/Rauschen"
Unbedingt eine Klasse für **Hintergrundgeräusche** einbauen (Wind, Regen, Menschenstimmen, Stille). Ohne diese klassifiziert das Modell *alles* als Vogel.

---

## 🔊 Schritt 2: Audio vorbereiten

Edge Impulse erwartet **WAV, 16 kHz, Mono**. Konvertierung mit `ffmpeg`:

```bash
# Alle MP3s eines Ordners in 16kHz-Mono-WAV konvertieren
for f in *.mp3; do
  ffmpeg -i "$f" -ar 16000 -ac 1 "converted/${f%.mp3}.wav"
done
```

---

## 🧠 Schritt 3: Modell trainieren mit Edge Impulse

### Empfohlene Einstellungen

| Parameter | Wert |
|---|---|
| Window size | 1000 ms |
| Window stride | 500 ms (50% overlap) |
| Sample rate | 16.000 Hz |
| DSP-Block | **MFE** (Mel Filterbank Energy) – besser als MFCC für Vogelrufe |
| MFE Filter | 40 |
| FFT-Länge | 512 |
| Modell | 2D CNN (voreingestellt für Audio) |
| Quantisierung | **Int8** (für MCU zwingend) |

### Modellgröße reduzieren falls nötig
Wenn das Modell nach dem Training zu groß ist:
- Weniger CNN-Filter (z.B. von 32 auf 16 reduzieren)
- Kleineres Input-Fenster (z.B. 750 ms statt 1000 ms)

### Trainingspipeline im Überblick

```
xeno-canto MP3s
      ↓
  Resampling auf 16kHz Mono (ffmpeg)
      ↓
  Upload zu Edge Impulse (Web UI oder CLI)
      ↓
  DSP-Block: MFE (Mel Filterbank Energy)
      ↓
  Classifier: 2D CNN
      ↓
  Int8-Quantisierung → .tflite (~80–200 KB)
      ↓
  Export als ESP-IDF Library
```

---

## 🚀 Schritt 4: Deployment auf ESP32-S3

### Export aus Edge Impulse
`Deployment → "ESP-IDF Library"` (enthält fertiges `.tflite`, DSP-Code und Beispiel-Inferenz-Code)

### Inferenz-Loop auf dem Board

```
Audio aufnehmen (I2S)
      ↓
  DSP: MFE-Features berechnen
      ↓
  TFLite Micro Interpreter
      ↓
  Softmax → Label + Confidence ausgeben
      ↓
  (Optional: per MQTT/WiFi an SenseBox-Cloud senden)
```

### Framework
- **ESP-IDF** mit `esp-tflite-micro` Komponente von Espressif
- Alternativ: Arduino-Framework (einfacher, aber weniger Kontrolle)

---

## 🔄 Alternativen falls On-Device nicht reicht

| Szenario | Lösung |
|---|---|
| Volle BirdNET-Genauigkeit nötig | Raspberry Pi Zero 2W oder Pi 4 |
| Mehr Arten als 10–30 | ESP32-S3 sendet Audio-Clips per WiFi an Server mit BirdNET |
| Kompromiss | ESP32-S3 als "smarter Sensor" + MQTT an lokalen Pi-Server |

---

## 📋 Bekannte Fallstricke

- **Schlechte Audiodaten** sind der häufigste Fehler – MP3-Artefakte und Hintergrundgeräusch in Trainingsdaten stark vermeiden
- **Fehlendes Rausch-Label** führt dazu, dass alles als Vogel erkannt wird
- **PSRAM ist langsam**: Tensor-Arena möglichst ins SRAM (<300 KB) halten, nicht ins PSRAM
- **DSP-Pipeline muss identisch** sein zwischen Training (Edge Impulse) und Deployment – nicht einfach `.tflite`-Datei austauschen ohne DSP-Code zu aktualisieren

---

## 🔗 Nützliche Links

| Ressource | URL |
|---|---|
| BirdNET-Lite (TFLite-Modell) | https://github.com/birdnet-team/BirdNET-Lite |
| Edge Impulse | https://studio.edgeimpulse.com |
| xeno-canto Vogelrufe | https://xeno-canto.org |
| esp-tflite-micro | https://github.com/espressif/esp-tflite-micro |
| ESP-IDF Dokumentation | https://docs.espressif.com/projects/esp-idf |
| Macaulay Library | https://www.macaulaylibrary.org |

---

*Erstellt mit Claude – Anthropic | Projektbeginn 2026*