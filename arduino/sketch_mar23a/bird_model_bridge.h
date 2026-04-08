#ifndef BIRD_MODEL_BRIDGE_H
#define BIRD_MODEL_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#define BIRD_LABEL_COUNT 2

const char* const* bird_labels();
size_t bird_label_count();
size_t bird_dsp_input_frame_size();
uint32_t bird_frequency();
int bird_impulse_ok();
int run_bird_classifier(const int16_t *audio_data, size_t audio_length, float *scores_out, size_t scores_len);

#endif // BIRD_MODEL_BRIDGE_H