#ifndef TEST_TONE_H
#define TEST_TONE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t phase;
} test_tone_t;
void test_tone_init(test_tone_t *tone);
/* 500 Hz left / 1 kHz right at 32 kHz, peak 4096 (~-18 dBFS).
 * Phase continues during digital silence to keep block behavior deterministic. */
void test_tone_render(test_tone_t *tone, uint32_t *frames, size_t count, bool audible);
#endif
