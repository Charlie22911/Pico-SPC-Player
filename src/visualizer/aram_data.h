#ifndef VISUALIZER_ARAM_DATA_H
#define VISUALIZER_ARAM_DATA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#define ARAM_DATA_PIXELS_BYTES (65536u / 2u)

typedef enum {
    ARAM_DATA_FREE = 0u,
    ARAM_DATA_WRITING,
    ARAM_DATA_READY,
    ARAM_DATA_READING,
} aram_data_state_t;

typedef struct {
    const uint8_t *pixels;
    uint32_t request;
    uint32_t generation;
    uint32_t sequence;
    bool held;
} aram_data_snapshot_t;

typedef struct {
    _Atomic uint32_t state;
    uint32_t request;
    uint32_t generation;
    uint32_t sequence;
    uint8_t pixels[ARAM_DATA_PIXELS_BYTES];
} aram_data_mailbox_t;

/* Core 0 writes and Core 1 reads this single-slot copied heatmap. The high
 * nibble is the lower/even ARAM address. Request and generation tags let Core 1
 * reject stale mode or track data. Acquisition and publication never wait. */

void aram_data_init(aram_data_mailbox_t *mailbox);
uint8_t *aram_data_begin_write(aram_data_mailbox_t *mailbox);
void aram_data_publish(aram_data_mailbox_t *mailbox, uint32_t request, uint32_t generation,
                       uint32_t sequence);
void aram_data_cancel_write(aram_data_mailbox_t *mailbox);
bool aram_data_acquire(aram_data_mailbox_t *mailbox, aram_data_snapshot_t *snapshot);
void aram_data_release(aram_data_mailbox_t *mailbox, aram_data_snapshot_t *snapshot);

#endif
