#include "psram_validation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fake_memory {
    uint8_t *bytes;
    size_t physical_size;
    bool fail_reads;
    bool fail_writes;
    bool corrupt_adjacent_word;
} fake_memory_t;

static int failures;

static void expect_size(const char *name, size_t actual, size_t expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: got %zu, expected %zu\n", name, actual, expected);
        ++failures;
    }
}

static void expect_true(const char *name, bool value) {
    if (!value) {
        fprintf(stderr, "%s: expected true\n", name);
        ++failures;
    }
}

static void expect_false(const char *name, bool value) {
    if (value) {
        fprintf(stderr, "%s: expected false\n", name);
        ++failures;
    }
}

static bool fake_read(void *context, size_t offset, uint32_t *value) {
    fake_memory_t *memory = context;
    if (memory->fail_reads || value == NULL || memory->physical_size < sizeof(*value)) {
        return false;
    }
    const size_t physical_offset = offset % memory->physical_size;
    if (physical_offset > memory->physical_size - sizeof(*value)) {
        return false;
    }
    memcpy(value, memory->bytes + physical_offset, sizeof(*value));
    return true;
}

static bool fake_write(void *context, size_t offset, uint32_t value) {
    fake_memory_t *memory = context;
    if (memory->fail_writes || memory->physical_size < sizeof(value)) {
        return false;
    }
    const size_t physical_offset = offset % memory->physical_size;
    if (physical_offset > memory->physical_size - sizeof(value)) {
        return false;
    }
    memcpy(memory->bytes + physical_offset, &value, sizeof(value));
    if (memory->corrupt_adjacent_word && physical_offset == sizeof(uint32_t)) {
        memory->bytes[physical_offset] ^= 0x80u;
    }
    return true;
}

static board_psram_access_t access_for(fake_memory_t *memory) {
    board_psram_access_t access = {
        .context = memory,
        .read_word = fake_read,
        .write_word = fake_write,
    };
    return access;
}

static void test_capacity_is_capped_to_two_mib(void) {
    expect_size("8 MiB ID is capped", board_psram_capacity_from_id(0x5du, 0x26u),
                2u * 1024u * 1024u);
    expect_size("4 MiB ID is capped", board_psram_capacity_from_id(0x5du, 0x20u),
                2u * 1024u * 1024u);
}

static void test_capacity_rejects_unknown_manufacturer(void) {
    expect_size("unknown manufacturer", board_psram_capacity_from_id(0x00u, 0x26u), 0u);
}

static void test_one_mib_capacity_remains_one_mib(void) {
    expect_size("1 MiB ID", board_psram_capacity_from_id(0x5du, 0x00u), 1u * 1024u * 1024u);
}

static void test_validation_accepts_distinct_memory_and_restores_words(void) {
    fake_memory_t memory = {
        .bytes = malloc(BOARD_PSRAM_CAPACITY_LIMIT_BYTES),
        .physical_size = BOARD_PSRAM_CAPACITY_LIMIT_BYTES,
    };
    if (memory.bytes == NULL) {
        fprintf(stderr, "allocation failed\n");
        ++failures;
        return;
    }
    memset(memory.bytes, 0xa5, memory.physical_size);
    uint8_t *before = malloc(memory.physical_size);
    if (before == NULL) {
        free(memory.bytes);
        ++failures;
        return;
    }
    for (size_t i = 0u; i < memory.physical_size; ++i) {
        memory.bytes[i] = (uint8_t)(i * 31u + (i >> 8u));
    }
    memcpy(before, memory.bytes, memory.physical_size);

    board_psram_access_t access = access_for(&memory);
    expect_true("2 MiB validation",
                board_psram_validate_memory(&access, BOARD_PSRAM_CAPACITY_LIMIT_BYTES));

    expect_true("validation restores every byte", memcmp(before, memory.bytes, memory.physical_size) == 0);
    free(before);
    free(memory.bytes);
}

static void test_validation_rejects_aliasing_memory(void) {
    fake_memory_t memory = {
        .bytes = calloc(1u, 1024u * 1024u),
        .physical_size = 1024u * 1024u,
    };
    if (memory.bytes == NULL) {
        fprintf(stderr, "allocation failed\n");
        ++failures;
        return;
    }
    board_psram_access_t access = access_for(&memory);
    expect_false("1 MiB aliases inside claimed 2 MiB",
                 board_psram_validate_memory(&access, BOARD_PSRAM_CAPACITY_LIMIT_BYTES));
    free(memory.bytes);
}

static void test_validation_rejects_adjacent_word_corruption(void) {
    fake_memory_t memory = {
        .bytes = calloc(1u, BOARD_PSRAM_CAPACITY_LIMIT_BYTES),
        .physical_size = BOARD_PSRAM_CAPACITY_LIMIT_BYTES,
        .corrupt_adjacent_word = true,
    };
    if (memory.bytes == NULL) {
        ++failures;
        return;
    }
    board_psram_access_t access = access_for(&memory);
    expect_false("adjacent word corruption",
                 board_psram_validate_memory(&access, memory.physical_size));
    free(memory.bytes);
}

static void test_validation_rejects_invalid_or_failed_access(void) {
    fake_memory_t memory = {
        .bytes = calloc(1u, BOARD_PSRAM_CAPACITY_LIMIT_BYTES),
        .physical_size = BOARD_PSRAM_CAPACITY_LIMIT_BYTES,
        .fail_reads = true,
    };
    if (memory.bytes == NULL) {
        fprintf(stderr, "allocation failed\n");
        ++failures;
        return;
    }
    board_psram_access_t access = access_for(&memory);
    expect_false("read failure", board_psram_validate_memory(&access, memory.physical_size));
    expect_false("null access", board_psram_validate_memory(NULL, memory.physical_size));
    expect_false("undersized capacity", board_psram_validate_memory(&access, sizeof(uint32_t)));
    free(memory.bytes);
}

int main(void) {
    test_capacity_is_capped_to_two_mib();
    test_capacity_rejects_unknown_manufacturer();
    test_one_mib_capacity_remains_one_mib();
    test_validation_accepts_distinct_memory_and_restores_words();
    test_validation_rejects_aliasing_memory();
    test_validation_rejects_adjacent_word_corruption();
    test_validation_rejects_invalid_or_failed_access();

    if (failures != 0) {
        fprintf(stderr, "%d board PSRAM assertion(s) failed\n", failures);
        return 1;
    }
    puts("board PSRAM tests passed");
    return 0;
}
