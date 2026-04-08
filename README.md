# MEISE - Micro Environmental Intelligence Singing Engine

Passive acoustic bird monitoring on a low-power embedded device. A senseBox Eye (ESP32-S3) paired with an Adafruit I2S microphone listens continuously and runs two TinyML classifiers on-device: one to detect whether a bird is vocalising, and a second to identify the species. No cloud connectivity required.

Seven Central European garden species are supported: *Amsel, Blaumeise, Heckenbraunelle, Zaunkönig, Rotkehlchen, Kohlmeise, Mönchsgrasmücke.*

---

## Repository Structure

```
arduino/
  sketch_mar23a/              Main ESP32 sketch + Edge Impulse model bridges
  bird_detection.zip          Edge Impulse export – binary bird/no-bird model
  species_classification.zip  Edge Impulse export – 7-species model

audio snippets/
  xeno canto audio files/     Raw recordings per species (used for training)
  Freesound audio files/      Non-bird background audio (wind, rain, people)
  snippet_extraction.py       Cuts 5-second RMS-filtered snippets for training

3D-MODEL/                     STL files for the birdhouse housing (TinkerCAD)
POSTER/                       Project poster (MEISE-IoT-A0.pdf) and QR code script

save_audio_from_i2s.py        Receives a WAV recording from the ESP32 over serial
```

## How It Works

1. **Data preparation** - `snippet_extraction.py` loads recordings from xeno-canto (birds) and Freesound (non-bird noise), resamples to 16 kHz, and extracts 5-second snippets centred on energy peaks using RMS filtering.
2. **Model training** - Both classifiers are trained in Edge Impulse using MFE spectrograms and exported as C++ libraries.
3. **On-device inference** - The sketch captures a 2-second I2S audio window (32 000 samples), removes the DC offset, and runs the bird detection model. If confidence exceeds 0.8, the species model runs on the same window. Results are colour-coded on the RGB LED and appended to `bird_log.csv` on the SD card.

## Hardware

| Component | Role |
|---|---|
| senseBox Eye (ESP32-S3) | Microcontroller + SD card |
| Adafruit I2S MEMS Mic | Audio capture at 16 kHz |
| 3D-printed housing | Weather protection (see `3D-MODEL/`) |


## Contact
Lukas Räuschel (lraeusch@uni-muenster.de)
Mika Dinnus (mdinnus@uni-muenster.de)