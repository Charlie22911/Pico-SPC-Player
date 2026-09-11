#ifndef VISUALIZER_VISUAL_RATE_H
#define VISUALIZER_VISUAL_RATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t requested_hz;
    uint32_t phase;
} visual_rate_t;

void visual_rate_set(visual_rate_t *rate, uint8_t requested_hz);
bool visual_rate_advance(visual_rate_t *rate, uint32_t audio_frames);

#endif
