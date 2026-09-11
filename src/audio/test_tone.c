#include "test_tone.h"
#include "pcm_format.h"
static const int16_t sine64[64] = {
    0,     401,   799,   1189,  1567,  1931,  2276,  2598,  2896,  3166,  3406,  3612,  3784,
    3920,  4017,  4076,  4096,  4076,  4017,  3920,  3784,  3612,  3406,  3166,  2896,  2598,
    2276,  1931,  1567,  1189,  799,   401,   0,     -401,  -799,  -1189, -1567, -1931, -2276,
    -2598, -2896, -3166, -3406, -3612, -3784, -3920, -4017, -4076, -4096, -4076, -4017, -3920,
    -3784, -3612, -3406, -3166, -2896, -2598, -2276, -1931, -1567, -1189, -799,  -401};
void test_tone_init(test_tone_t *tone) {
    tone->phase = 0;
}
void test_tone_render(test_tone_t *tone, uint32_t *frames, size_t count, bool audible) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t phase = tone->phase;
        frames[i] = audible ? pcm_pack_i2s(sine64[phase], sine64[(phase * 2u) & 63u]) : 0u;
        tone->phase = (phase + 1u) & 63u;
    }
}
