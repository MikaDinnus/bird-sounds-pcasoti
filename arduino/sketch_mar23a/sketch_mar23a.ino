#include <driver/i2s.h>
#include <string.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "bird_model_bridge.h"
#include "species_model_bridge.h"

// SD card wiring for the VSPI bus on the ESP32-S3 board.
#define VSPI_MISO MISO
#define VSPI_MOSI MOSI
#define VSPI_SCLK SCK
#define VSPI_SS PIN_SD_CS
SPIClass sdspi = SPIClass();

// I2S microphone pins for the SenseBox ESP32-S3.
#define I2S_WS   2
#define I2S_SCK  14
#define I2S_SD   48

// I2S capture configuration.
#define I2S_PORT          I2S_NUM_0
#define SAMPLE_RATE       16000
#define SAMPLE_BITS       I2S_BITS_PER_SAMPLE_32BIT
#define DMA_BUF_COUNT     8
#define DMA_BUF_LEN       64

// The Edge Impulse models both expect 32,000 raw audio samples per window.
constexpr size_t AUDIO_BUFFER_SIZE = 32000;
constexpr float BIRD_DETECTION_THRESHOLD = 0.8f;
constexpr uint32_t I2S_STABILIZATION_DELAY_MS = 500;
constexpr uint32_t INFERENCE_PAUSE_MS = 200;
constexpr size_t I2S_DUMMY_READS = 10;
constexpr uint32_t SERIAL_WAIT_TIMEOUT_MS = 2000;
constexpr uint32_t DETECTION_FLASH_MS = 120;

void setLedBirdDetected(bool birdDetected) {
    if (birdDetected) {
        rgbLedWrite(PIN_LED, 0x00, 0x10, 0x00);
    } else {
        rgbLedWrite(PIN_LED, 0x00, 0x00, 0x00);
    }
}

size_t argmaxScore(const float *scores, size_t count) {
    size_t max_idx = 0;
    float max_val = scores[0];
    for (size_t i = 1; i < count; i++) {
        if (scores[i] > max_val) {
            max_val = scores[i];
            max_idx = i;
        }
    }
    return max_idx;
}

void setLedColorForSpecies(size_t speciesIndex) {
    switch (speciesIndex) {
        case 0: rgbLedWrite(PIN_LED, 0x10, 0x08, 0x00); break; // orange
        case 1: rgbLedWrite(PIN_LED, 0x00, 0x00, 0x10); break; // blue
        case 2: rgbLedWrite(PIN_LED, 0x10, 0x10, 0x00); break; // yellow
        case 3: rgbLedWrite(PIN_LED, 0x10, 0x00, 0x10); break; // magenta
        case 4: rgbLedWrite(PIN_LED, 0x00, 0x10, 0x10); break; // cyan
        case 5: rgbLedWrite(PIN_LED, 0x10, 0x00, 0x00); break; // red
        case 6: rgbLedWrite(PIN_LED, 0x10, 0x10, 0x10); break; // white
        default: rgbLedWrite(PIN_LED, 0x10, 0x00, 0x00); break;
    }
}

// Converted 16-bit audio samples used by both models.
int16_t audio_buffer[AUDIO_BUFFER_SIZE];

// Edge Impulse calls this callback to fetch the next chunk of audio samples.
int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)audio_buffer[offset + i];
    }
    return 0;
}

// Append one detection row to the CSV log on the SD card.
void logToSD(float bird_score, float *species_scores) {
    File file = SD.open("/bird_log.csv", FILE_APPEND);

    if (!file) {
        Serial.println("[SD] Failed to open /bird_log.csv for appending.");
        return;
    }

    file.print(millis());
    file.print(",");
    file.print(bird_score, 3);

    for (size_t i = 0; i < species_label_count(); i++) {
        file.print(",");
        file.print(species_scores[i], 3);
    }

    file.println();
    file.close();
}

// Create the CSV header once when the log file does not exist yet.
void ensureLogFileHeader() {
    if (SD.exists("/bird_log.csv")) {
        return;
    }

    File file = SD.open("/bird_log.csv", FILE_WRITE);
    if (!file) {
        Serial.println("[SD] Failed to create /bird_log.csv.");
        return;
    }

    file.print("time,bird_score");
    for (size_t i = 0; i < species_label_count(); i++) {
        file.print(",");
        file.print(species_labels()[i]);
    }
    file.println();
    file.close();
}

// Read one full audio window from I2S and convert 32-bit samples into 16-bit PCM.
void readAudioWindow() {
    int32_t raw_block[256];
    size_t bytes_read = 0;
    size_t write_index = 0;

    while (write_index < AUDIO_BUFFER_SIZE) {
        const size_t remaining = AUDIO_BUFFER_SIZE - write_index;
        const int samples_to_read = (remaining > 256u) ? 256 : (int)remaining;

        i2s_read(I2S_PORT, raw_block, samples_to_read * sizeof(int32_t), &bytes_read, 100);

        const size_t samples_got = bytes_read / sizeof(int32_t);
        for (size_t i = 0; i < samples_got && write_index < AUDIO_BUFFER_SIZE; i++) {
            audio_buffer[write_index++] = (int16_t)(raw_block[i] >> 16);
        }
    }
}

// Remove the DC offset from the captured audio so both classifiers see centered samples.
void removeDcOffset() {
    long long sum = 0;
    for (size_t i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        sum += audio_buffer[i];
    }

    const int16_t dc_offset = (int16_t)(sum / AUDIO_BUFFER_SIZE);
    for (size_t i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        audio_buffer[i] -= dc_offset;
    }
}

void printScores(const char *prefix, const char *const *labels, const float *scores, size_t label_count) {
    Serial.print(prefix);
    for (size_t i = 0; i < label_count; i++) {
        Serial.printf("%s=%.3f  ", labels[i], scores[i]);
    }
    Serial.println();
}


void setup() {
    Serial.begin(115200);
    const uint32_t serial_wait_start = millis();
    while (!Serial && (millis() - serial_wait_start) < SERIAL_WAIT_TIMEOUT_MS) {
        delay(10);
    }
    delay(2000);

    Serial.println("=== MEISE Bird Detection ===");
    Serial.printf("Audio window: %u samples @ %u Hz = %u ms\n",
                  (unsigned)bird_dsp_input_frame_size(),
                  (unsigned)bird_frequency(),
                  (unsigned)(bird_dsp_input_frame_size() * 1000 / bird_frequency()));
    Serial.printf("Bird model labels: %u -> ", (unsigned)bird_label_count());
    for (size_t i = 0; i < bird_label_count(); i++) {
        Serial.printf("[%s] ", bird_labels()[i]);
    }
    Serial.println("\n---");

    // Default state: no bird detected -> LED off.
    setLedBirdDetected(false);

    // Start the I2S peripheral for microphone capture.
    const i2s_config_t i2s_config = {
        .mode                  = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate           = SAMPLE_RATE,
        .bits_per_sample       = SAMPLE_BITS,
        .channel_format        = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format  = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags      = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count         = DMA_BUF_COUNT,
        .dma_buf_len           = DMA_BUF_LEN,
        .use_apll              = false,
        .tx_desc_auto_clear    = false,
        .fixed_mclk            = 0
    };

    const i2s_pin_config_t pin_config = {
        .bck_io_num    = I2S_SCK,
        .ws_io_num     = I2S_WS,
        .data_out_num  = I2S_PIN_NO_CHANGE,
        .data_in_num   = I2S_SD
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S] i2s_driver_install failed (%d)\n", err);
        while (1) delay(1000);
    }
    if (i2s_set_pin(I2S_PORT, &pin_config) != ESP_OK) {
        Serial.println("[I2S] i2s_set_pin failed.");
        while (1) delay(1000);
    }

    i2s_zero_dma_buffer(I2S_PORT);

    // Mount the SD card and prepare the CSV log.
    pinMode(SD_ENABLE, OUTPUT);
    digitalWrite(SD_ENABLE, LOW);
    sdspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, VSPI_SS);

    if (!SD.begin(VSPI_SS, sdspi)) {
        Serial.println("[SD] Card mount failed.");
        rgbLedWrite(PIN_LED, 0x10, 0x10, 0x00);
        while (1);
    }

    ensureLogFileHeader();

    // Discard the first few reads while the microphone settles after power-on.
    delay(I2S_STABILIZATION_DELAY_MS);
    int32_t dummy[64];
    size_t dummy_bytes;
    for (size_t i = 0; i < I2S_DUMMY_READS; i++) {
        i2s_read(I2S_PORT, dummy, sizeof(dummy), &dummy_bytes, 100);
    }

    Serial.println("[Init] I2S ready. Starting inference loop.");
    Serial.println("====================================");
}

void loop() {
    // Capture one full window and normalize it before inference.
    readAudioWindow();
    removeDcOffset();

    float bird_scores[BIRD_LABEL_COUNT] = {0.0f};
    int ei_err = run_bird_classifier(audio_buffer, AUDIO_BUFFER_SIZE, bird_scores, bird_label_count());

    if (ei_err != bird_impulse_ok()) {
        Serial.printf("[Bird Model] Error: %d\n", ei_err);
        setLedBirdDetected(false);
        delay(INFERENCE_PAUSE_MS);
        return;
    }

    float bird_score = 0.0f;

    for (size_t i = 0; i < bird_label_count(); i++) {
        if (strcmp(bird_labels()[i], "bird") == 0) {
            bird_score = bird_scores[i];
            break;
        }
    }

    printScores("[Bird Model] ", bird_labels(), bird_scores, bird_label_count());

    const bool birdDetected = (bird_score > BIRD_DETECTION_THRESHOLD);

    // Only run the species model when the bird model is confident enough.
    if (birdDetected) {

        Serial.printf("[Species Check] audio=%u expected=%u labels=%u\n",
                      (unsigned)AUDIO_BUFFER_SIZE,
                      (unsigned)species_dsp_input_frame_size(),
                      (unsigned)species_label_count());

        float species_scores[SPECIES_LABEL_COUNT] = {0.0f};
        int species_err = run_species_classifier(audio_buffer, AUDIO_BUFFER_SIZE, species_scores, species_label_count());

        if (species_err != species_impulse_ok()) {
            Serial.printf("[Species Model] Error: %d\n", species_err);
            setLedBirdDetected(false);
            delay(INFERENCE_PAUSE_MS);
            return;
        }

        // Brief detection flash, then the species-coded color.
        setLedBirdDetected(true);
        delay(DETECTION_FLASH_MS);

        const size_t top_species_idx = argmaxScore(species_scores, species_label_count());
        setLedColorForSpecies(top_species_idx);
        Serial.printf("[LED] Species color: %s\n", species_labels()[top_species_idx]);

        logToSD(bird_score, species_scores);

        printScores(">>> Species: ", species_labels(), species_scores, species_label_count());
    } else {
        setLedBirdDetected(false);
        Serial.printf("[Species Model] Skipped because bird confidence is only %.3f.\n",
                      bird_score);
    }

    delay(INFERENCE_PAUSE_MS);
}