#ifndef ARAM_ACTIVITY_H
#define ARAM_ACTIVITY_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#define ARAM_ACTIVITY_ADDRESS_COUNT 65536u
#define ARAM_ACTIVITY_BITMAP_BYTES (ARAM_ACTIVITY_ADDRESS_COUNT / 8u)
#define ARAM_ACTIVITY_BANK_COUNT 2u

typedef enum {
    ARAM_ACTIVITY_BANK_FREE = 0u,
    ARAM_ACTIVITY_BANK_WRITING,
    ARAM_ACTIVITY_BANK_READY,
    ARAM_ACTIVITY_BANK_READING,
} aram_activity_bank_state_t;

typedef struct {
    uint8_t read[ARAM_ACTIVITY_BITMAP_BYTES];
    uint8_t write[ARAM_ACTIVITY_BITMAP_BYTES];
    uint8_t execute[ARAM_ACTIVITY_BITMAP_BYTES];
    uint32_t generation;
    uint32_t sequence;
} aram_activity_bank_t;

typedef struct {
    aram_activity_bank_t banks[ARAM_ACTIVITY_BANK_COUNT];
    _Atomic uint32_t states[ARAM_ACTIVITY_BANK_COUNT];
    uint32_t producer_bank;
    uint32_t generation;
    uint32_t sequence;
} aram_activity_t;

typedef struct {
    const uint8_t *read;
    const uint8_t *write;
    const uint8_t *execute;
    uint32_t generation;
    uint32_t sequence;
    uint32_t bank;
    bool held;
} aram_activity_snapshot_t;

/* Core 0 writes one bank while Core 1 may hold the other. Each bitmap contains
 * one bit per 16-bit ARAM address. Publication/acquisition never waits. */

void aram_activity_init(aram_activity_t *activity, uint32_t generation);
void aram_activity_reset_generation(aram_activity_t *activity, uint32_t generation);
void aram_activity_producer_maps(aram_activity_t *activity, uint8_t **read, uint8_t **write,
                                 uint8_t **execute);
bool aram_activity_publish(aram_activity_t *activity);
bool aram_activity_acquire(aram_activity_t *activity, aram_activity_snapshot_t *snapshot);
void aram_activity_release(aram_activity_t *activity, aram_activity_snapshot_t *snapshot);

static inline void aram_activity_mark(uint8_t *bitmap, uint16_t address) {
    bitmap[address >> 3u] |= (uint8_t)(1u << (address & 7u));
}

static inline bool aram_activity_test_bit(const uint8_t *bitmap, uint16_t address) {
    return (bitmap[address >> 3u] & (uint8_t)(1u << (address & 7u))) != 0u;
}

#endif
