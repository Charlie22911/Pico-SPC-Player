#ifndef STORAGE_STORAGE_MAILBOX_H
#define STORAGE_STORAGE_MAILBOX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "spc_catalog.h"

typedef enum {
    STORAGE_MAILBOX_EMPTY = 0,
    STORAGE_MAILBOX_WRITING,
    STORAGE_MAILBOX_READY,
    STORAGE_MAILBOX_READING,
    STORAGE_MAILBOX_COMPLETE,
} storage_mailbox_state_t;

/* Core 1 owns the image while EMPTY/WRITING/READY. Core 0 owns it while
 * READING, then publishes COMPLETE with the accepted generation. Core 1 must
 * reclaim COMPLETE before reusing the fixed-capacity byte buffer. All calls are nonblocking. */
typedef struct {
    _Atomic uint32_t state;
    size_t size;
    spc_track_ref_t track;
    bool accepted;
    uint32_t generation;
    uint8_t image[SPC_CORE_LOAD_BYTES];
} storage_mailbox_t;

void storage_mailbox_init(storage_mailbox_t *mailbox);
uint8_t *storage_mailbox_begin_write(storage_mailbox_t *mailbox);
void storage_mailbox_cancel_write(storage_mailbox_t *mailbox);
void storage_mailbox_publish(storage_mailbox_t *mailbox, size_t size, spc_track_ref_t track);
bool storage_mailbox_begin_read(storage_mailbox_t *mailbox, const uint8_t **image, size_t *size,
                                spc_track_ref_t *track);
void storage_mailbox_complete_read(storage_mailbox_t *mailbox, bool accepted, uint32_t generation);
bool storage_mailbox_reclaim(storage_mailbox_t *mailbox, bool *accepted, uint32_t *generation);

#endif
