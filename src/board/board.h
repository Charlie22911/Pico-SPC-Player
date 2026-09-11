#ifndef BOARD_BOARD_H
#define BOARD_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    BOARD_OK = 0,
    BOARD_ERROR_CLOCK,
    BOARD_ERROR_PSRAM,
    BOARD_ERROR_RESOURCE,
    BOARD_ERROR_WRONG_CORE,
    BOARD_ERROR_NOT_INITIALIZED,
} board_result_t;

typedef struct {
    uint32_t sys_clock_hz;
    uint32_t peri_clock_hz;
    size_t psram_capacity_bytes;
    bool clock_ready;
    bool psram_ready;
    bool display_ready;
    bool touch_ready;
    board_result_t last_error;
} board_status_t;

board_result_t board_init(void);
board_result_t board_ui_init(void);
board_status_t board_get_status(void);
bool board_touch_read(uint16_t *x, uint16_t *y, bool *pressed);

#endif
