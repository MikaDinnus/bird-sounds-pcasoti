#include "bird_model_bridge.h"

#include <MEISE_bird_or_no_bird_inferencing.h>

namespace {
const int16_t *g_bird_audio = nullptr;

int bird_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)g_bird_audio[offset + i];
    }
    return 0;
}
}

const char* const* bird_labels() {
    return bird_or_no_bird_ei_classifier_inferencing_categories;
}

size_t bird_label_count() {
    return EI_CLASSIFIER_LABEL_COUNT;
}

size_t bird_dsp_input_frame_size() {
    return EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
}

uint32_t bird_frequency() {
    return EI_CLASSIFIER_FREQUENCY;
}

int bird_impulse_ok() {
    return EI_IMPULSE_OK;
}

int run_bird_classifier(const int16_t *audio_data, size_t audio_length, float *scores_out, size_t scores_len) {
    if (audio_data == nullptr || scores_out == nullptr) {
        return EI_IMPULSE_INFERENCE_ERROR;
    }
    if (audio_length != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE || scores_len < EI_CLASSIFIER_LABEL_COUNT) {
        return EI_IMPULSE_INVALID_SIZE;
    }

    g_bird_audio = audio_data;

    signal_t signal;
    signal.total_length = audio_length;
    signal.get_data = &bird_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR err = bird_or_no_bird_run_classifier(&signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        return err;
    }

    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        scores_out[i] = result.classification[i].value;
    }

    return EI_IMPULSE_OK;
}