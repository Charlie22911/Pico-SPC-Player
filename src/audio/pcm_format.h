#ifndef PCM_FORMAT_H
#define PCM_FORMAT_H
#include <stdbool.h>
#include <stdint.h>
#define PCM_SAMPLE_RATE 32000u
#define PCM_BLOCK_FRAMES 256u
uint32_t pcm_pack_i2s(int16_t left, int16_t right);
uint16_t pcm_peak_magnitude(int16_t sample);
/* Divider in unsigned 16.8 fixed point, for a 64-instruction stereo frame. */
bool pcm_i2s_divider(uint32_t system_hz, uint32_t frame_hz, uint32_t *divider);
#endif
