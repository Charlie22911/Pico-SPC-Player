#include "visual_rate.h"

#include <stddef.h>

#define VISUAL_SAMPLE_RATE 32000u

void visual_rate_set(visual_rate_t *rate, uint8_t requested_hz) {
    if (rate == NULL || (requested_hz != 30u && requested_hz != 60u))
        return;
    if (rate->requested_hz != requested_hz) {
        rate->requested_hz = requested_hz;
        rate->phase = 0u;
    }
}

bool visual_rate_advance(visual_rate_t *rate, uint32_t audio_frames) {
    if (rate == NULL || (rate->requested_hz != 30u && rate->requested_hz != 60u))
        return false;
    rate->phase += audio_frames * rate->requested_hz;
    if (rate->phase < VISUAL_SAMPLE_RATE)
        return false;
    rate->phase -= VISUAL_SAMPLE_RATE;
    return true;
}
