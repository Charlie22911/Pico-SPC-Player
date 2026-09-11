#ifndef STORAGE_SD_CARD_H
#define STORAGE_SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SD_CARD_ABSENT = 0,
    SD_CARD_READY,
    SD_CARD_INIT_ERROR,
} sd_card_status_t;

sd_card_status_t sd_card_init(void);
bool sd_card_present(void);
bool sd_card_ready(void);
bool sd_card_read_blocks(uint32_t first_lba, uint8_t *destination, uint32_t block_count);

#endif
