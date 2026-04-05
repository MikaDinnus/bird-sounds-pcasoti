import os
import librosa
import soundfile as sf
import numpy as np
from sqlalchemy import false

# Set to True to process test data instead of full dataset
TEST = False


# Base paths
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_FOLDER = os.path.join(BASE_DIR, "xeno canto audio files")
TEST_INPUT_FOLDER = os.path.join(BASE_DIR, "test data")
OUTPUT_FOLDER = os.path.join(BASE_DIR, "snippets")
TEST_OUTPUT_FOLDER = os.path.join(BASE_DIR, "audio snippets/test")

# Parameters
SNIPPET_LENGTH = 5      # seconds
SAMPLE_RATE = 16000
MIN_GAP = 2             # minimum seconds between snippets
RMS_PERCENTILE = 75

os.makedirs(OUTPUT_FOLDER, exist_ok=True)
os.makedirs(TEST_OUTPUT_FOLDER, exist_ok=True)


def extract_snippets(file_path, species, counter, output_folder):
    print("Processing:", file_path)

    # load audio
    y, sr = librosa.load(file_path, sr=SAMPLE_RATE, mono=True)

    # compute RMS energy
    rms = librosa.feature.rms(y=y)[0]

    frames = range(len(rms))
    times = librosa.frames_to_time(frames, sr=sr)

    threshold = np.percentile(rms, RMS_PERCENTILE)
    peaks = times[rms > threshold]

    # filter peaks (avoid duplicates)
    filtered_peaks = []
    last_peak = -999

    for t in peaks:
        if t - last_peak > MIN_GAP:
            filtered_peaks.append(t)
            last_peak = t

    # output folder for species
    safe_species = species.replace(" ", "_")
    species_output_folder = os.path.join(output_folder, safe_species)
    os.makedirs(species_output_folder, exist_ok=True)

    # create snippets
    for t in filtered_peaks:

        start = int((t - SNIPPET_LENGTH / 2) * sr)
        end = int((t + SNIPPET_LENGTH / 2) * sr)

        if start < 0 or end > len(y):
            continue

        snippet = y[start:end]

        if len(snippet) < SNIPPET_LENGTH * sr:
            continue

        output_file = os.path.join(
            species_output_folder,
            f"{safe_species}_test_{counter}.wav"
        )

        sf.write(output_file, snippet, sr)

        counter += 1

    return counter


def process_dataset():

    if TEST:
        input_folder = TEST_INPUT_FOLDER
        output_folder = TEST_OUTPUT_FOLDER
    else:
        input_folder = INPUT_FOLDER
        output_folder = OUTPUT_FOLDER

    species_list = [
        d for d in os.listdir(input_folder)
        if os.path.isdir(os.path.join(input_folder, d))
    ]

    print("Detected species folders:")
    for s in species_list:
        print(" -", s)

    for species in species_list:

        counter = 1

        species_folder = os.path.join(input_folder, species)

        print("\nProcessing species:", species)
        print("Folder:", species_folder)

        for file_name in os.listdir(species_folder):

            if file_name.endswith(".wav") or file_name.endswith(".mp3"):

                file_path = os.path.join(species_folder, file_name)

                counter = extract_snippets(file_path, species, counter, output_folder)

def process_no_bird_audio():

    no_bird_folder = os.path.join(BASE_DIR, "Freesound audio files")
    output_folder = os.path.join(BASE_DIR, "audio snippets/no_bird")
    os.makedirs(output_folder, exist_ok=True)

    counter = 1

    for file_name in os.listdir(no_bird_folder):

        if file_name.endswith((".wav", ".mp3", ".aif", ".aiff", ".mp4")):

            file_path = os.path.join(no_bird_folder, file_name)

            y, sr = librosa.load(file_path, sr=SAMPLE_RATE, mono=True)

            total_length = len(y) / sr
            num_snippets = int(total_length // SNIPPET_LENGTH)

            for i in range(num_snippets):
                start = i * SNIPPET_LENGTH * sr
                end = start + SNIPPET_LENGTH * sr

                snippet = y[int(start):int(end)]

                output_file = os.path.join(
                    output_folder,
                    f"{file_name.split('.')[0]}_no_bird_{counter}.wav"
                )

                sf.write(output_file, snippet, sr)
                counter += 1

if __name__ == "__main__":
    # process_dataset()
    process_no_bird_audio()
