#ifndef BOARD_FLASH_CLOCK_H
#define BOARD_FLASH_CLOCK_H

#include <stdbool.h>

/* Core 0 startup only, before changing clk_sys or launching Core 1/DMA. */
bool board_flash_clock_init(void);

#endif
