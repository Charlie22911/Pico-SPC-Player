#include "pcm_format.h"
uint32_t pcm_pack_i2s(int16_t left, int16_t right) {
    return ((uint32_t)(uint16_t)left << 16) | (uint16_t)right;
}
uint16_t pcm_peak_magnitude(int16_t sample) {
    const int32_t value = sample;
    return (uint16_t)(value < 0 ? -value : value);
}
bool pcm_i2s_divider(uint32_t system_hz, uint32_t frame_hz, uint32_t *divider) {
    if (!divider || !frame_hz || !system_hz)
        return false;
    uint64_t instruction_hz = (uint64_t)frame_hz * 64u;
    uint64_t value = (((uint64_t)system_hz << 8) + instruction_hz / 2u) / instruction_hz;
    if (value < 256u || value > UINT32_C(0x00ffffff))
        return false;
    *divider = (uint32_t)value;
    return true;
}
