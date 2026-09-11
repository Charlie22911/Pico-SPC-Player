#ifndef BOARD_PSRAM_H
#define BOARD_PSRAM_H

#include <stddef.h>

#define BOARD_PSRAM_BASE ((void *)0x11000000u)

/* Initializes QMI CS1 and returns the validated usable capacity. */
size_t board_psram_init(void);

#endif
