#ifndef SPECIES_MODEL_BRIDGE_H
#define SPECIES_MODEL_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

// The species model has 7 classes in the exported model.
#define SPECIES_LABEL_COUNT 7

const char* const* species_labels();
size_t species_label_count();
size_t species_dsp_input_frame_size();
int species_impulse_ok();
int run_species_classifier(const int16_t *audio_data, size_t audio_length, float *scores_out, size_t scores_len);

#endif // SPECIES_MODEL_BRIDGE_H
