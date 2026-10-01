#ifndef BOARD_PSRAM_H
#define BOARD_PSRAM_H

#include <stddef.h>

#define BOARD_PSRAM_BASE ((void *)0x11000000u)
/* Fixed divider follows clk_sys; linear burst mode is specified up to 84 MHz. */
#define BOARD_PSRAM_CLKDIV 3u
#define BOARD_PSRAM_RXDELAY 4u
#define BOARD_PSRAM_COOLDOWN 1u

/* Initializes QMI CS1 and returns the validated usable capacity. */
size_t board_psram_init(void);

#endif
