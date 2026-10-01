#include "clock_validation.h"

uint32_t board_qmi_clock_divider(uint8_t encoded) {
    return encoded == 0u ? 256u : encoded;
}

static bool measured_clock_matches(uint32_t measured_khz, uint32_t requested_hz) {
    const uint64_t measured_hz = (uint64_t)measured_khz * 1000u;
    const uint64_t difference = measured_hz > requested_hz
                                    ? measured_hz - requested_hz
                                    : requested_hz - measured_hz;
    /* FC0 uses clk_ref as its reference; permit 0.1% measurement tolerance. */
    return requested_hz != 0u && measured_khz != 0u &&
           difference <= requested_hz / 1000u;
}

bool board_clocks_match(const board_clock_readings_t *clocks, uint32_t requested_hz,
                        uint32_t requested_flash_divider) {
    return clocks->configured_sys_hz == requested_hz &&
           clocks->configured_peri_hz == requested_hz &&
           measured_clock_matches(clocks->measured_sys_khz, requested_hz) &&
           measured_clock_matches(clocks->measured_peri_khz, requested_hz) &&
           board_qmi_clock_divider(clocks->flash_divider_encoded) == requested_flash_divider;
}
