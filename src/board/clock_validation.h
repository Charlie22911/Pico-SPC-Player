#ifndef BOARD_CLOCK_VALIDATION_H
#define BOARD_CLOCK_VALIDATION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t configured_sys_hz;
    uint32_t configured_peri_hz;
    uint32_t measured_sys_khz;
    uint32_t measured_peri_khz;
    uint8_t flash_divider_encoded;
} board_clock_readings_t;

uint32_t board_qmi_clock_divider(uint8_t encoded);
bool board_clocks_match(const board_clock_readings_t *clocks, uint32_t requested_hz,
                        uint32_t requested_flash_divider);

#endif
