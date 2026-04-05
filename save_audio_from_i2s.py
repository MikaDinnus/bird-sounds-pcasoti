import serial
import os

PORT = "COM8"        # Windows: COM3, COM4 … | Linux/Mac: /dev/ttyUSB0 o.ä.
BAUD = 115200
OUTPUT = "aufnahme.wav"

SAMPLE_RATE   = 16000
RECORD_TIME   = 2
AUDIO_SAMPLES = SAMPLE_RATE * RECORD_TIME
WAV_SIZE      = 44 + AUDIO_SAMPLES * 2  # 44 Byte Header + 16-bit Mono


ser = serial.Serial(PORT, BAUD, timeout=5)
print(f"Verbunden mit {PORT}, warte auf Aufnahme …")

while True:
    line = ser.readline().decode("utf-8", errors="ignore").strip()
    print(f"[ESP32] {line}")

    if line == "START_WAV":
        print("Empfange WAV-Daten …")
        data = ser.read(WAV_SIZE)

        if len(data) == WAV_SIZE:
            with open(OUTPUT, "wb") as f:
                f.write(data)
            print(f"✅ Gespeichert: {OUTPUT}  ({len(data)} Bytes)")
        else:
            print(f"⚠️  Unvollständig: {len(data)} von {WAV_SIZE} Bytes")
