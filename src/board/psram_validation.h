#ifndef BOARD_PSRAM_VALIDATION_H
#define BOARD_PSRAM_VALIDATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOARD_PSRAM_CAPACITY_LIMIT_BYTES (2u * 1024u * 1024u)

typedef bool (*board_psram_read_word_fn)(void *context, size_t offset, uint32_t *value);
typedef bool (*board_psram_write_word_fn)(void *context, size_t offset, uint32_t value);

typedef struct board_psram_access {
    void *context;
    board_psram_read_word_fn read_word;
    board_psram_write_word_fn write_word;
} board_psram_access_t;

size_t board_psram_capacity_from_id(uint8_t manufacturer_id, uint8_t extended_id);
bool board_psram_validate_memory(const board_psram_access_t *access, size_t capacity_bytes);

#endif
