#include "species_model_bridge.h"

// Rename second model globals/functions to avoid collisions with the bird/no-bird model.
#define ei_default_impulse species_ei_default_impulse
#define ei_classifier_inferencing_categories species_ei_classifier_inferencing_categories
#define ei_dsp_blocks_size species_ei_dsp_blocks_size
#define ei_dsp_blocks species_ei_dsp_blocks
#define run_classifier species_run_classifier_impl
#define run_classifier_init species_run_classifier_init_impl
#define run_classifier_deinit species_run_classifier_deinit_impl
#define run_classifier_continuous species_run_classifier_continuous_impl
#include <MEISE_inferencing.h>
#undef run_classifier_continuous
#undef run_classifier_deinit
#undef run_classifier_init
#undef run_classifier
#undef ei_dsp_blocks
#undef ei_dsp_blocks_size
#undef ei_classifier_inferencing_categories
#undef ei_default_impulse

namespace {
const int16_t *g_species_audio = nullptr;

int species_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)g_species_audio[offset + i];
    }
    return 0;
}
}

const char* const* species_labels() {
    return species_ei_classifier_inferencing_categories;
}

size_t species_label_count() {
    return SPECIES_LABEL_COUNT;
}

size_t species_dsp_input_frame_size() {
    return EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
}

int species_impulse_ok() {
    return EI_IMPULSE_OK;
}

int run_species_classifier(const int16_t *audio_data, size_t audio_length, float *scores_out, size_t scores_len) {
    if (audio_data == nullptr || scores_out == nullptr) {
        return EI_IMPULSE_INFERENCE_ERROR;
    }
    if (audio_length != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE || scores_len < SPECIES_LABEL_COUNT) {
        return EI_IMPULSE_INVALID_SIZE;
    }

    g_species_audio = audio_data;

    signal_t signal;
    signal.total_length = audio_length;
    signal.get_data = &species_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR err = species_run_classifier_impl(&signal, &result, false);
    if (err != EI_IMPULSE_OK) {
        return err;
    }

    for (size_t i = 0; i < SPECIES_LABEL_COUNT; i++) {
        scores_out[i] = result.classification[i].value;
    }

    return EI_IMPULSE_OK;
}
