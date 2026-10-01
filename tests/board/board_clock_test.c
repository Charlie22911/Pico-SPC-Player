#include <assert.h>
#include <stdio.h>

#include "clock_validation.h"

int main(void) {
    board_clock_readings_t clocks = {250000000u, 250000000u, 250000u, 250000u, 2u};
    assert(board_clocks_match(&clocks, 250000000u, 2u));

    /* Reject stale SDK bookkeeping, wrong measured clocks, and ROM flash settings. */
    clocks.configured_sys_hz = 150000000u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));
    clocks.configured_sys_hz = 250000000u;
    clocks.configured_peri_hz = 48000000u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));
    clocks.configured_peri_hz = 250000000u;
    clocks.measured_sys_khz = 150000u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));
    clocks.measured_sys_khz = 250000u;
    clocks.measured_peri_khz = 48000u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));
    clocks.measured_peri_khz = 250000u;
    clocks.flash_divider_encoded = 3u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));

    /* FC0 is quantized to kHz; allow 0.1% measurement tolerance. */
    clocks.flash_divider_encoded = 2u;
    clocks.measured_sys_khz = 249750u;
    clocks.measured_peri_khz = 250250u;
    assert(board_clocks_match(&clocks, 250000000u, 2u));
    clocks.measured_sys_khz = 249749u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));
    clocks.measured_sys_khz = 250000u;
    clocks.measured_peri_khz = 250251u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));
    clocks.measured_peri_khz = 0u;
    assert(!board_clocks_match(&clocks, 250000000u, 2u));

    /* QMI encodes divider 256 as zero, never as a stopped clock. */
    assert(board_qmi_clock_divider(0u) == 256u);
    assert(board_qmi_clock_divider(2u) == 2u);
    assert(board_qmi_clock_divider(255u) == 255u);
    clocks.measured_peri_khz = 250000u;
    clocks.flash_divider_encoded = 0u;
    assert(board_clocks_match(&clocks, 250000000u, 256u));
    assert(!board_clocks_match(&clocks, 250000000u, 2u));
    puts("board clock validation passed");
    return 0;
}
