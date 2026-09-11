#include "aram_data.h"

#include <stddef.h>

void aram_data_init(aram_data_mailbox_t *mailbox) {
    if (mailbox == NULL)
        return;
    atomic_init(&mailbox->state, ARAM_DATA_FREE);
    mailbox->request = 0u;
    mailbox->generation = 0u;
    mailbox->sequence = 0u;
}

uint8_t *aram_data_begin_write(aram_data_mailbox_t *mailbox) {
    if (mailbox == NULL)
        return NULL;
    uint32_t expected = ARAM_DATA_FREE;
    if (!atomic_compare_exchange_strong_explicit(&mailbox->state, &expected, ARAM_DATA_WRITING,
                                                 memory_order_acquire, memory_order_relaxed)) {
        return NULL;
    }
    return mailbox->pixels;
}

void aram_data_publish(aram_data_mailbox_t *mailbox, uint32_t request, uint32_t generation,
                       uint32_t sequence) {
    if (mailbox == NULL)
        return;
    mailbox->request = request;
    mailbox->generation = generation;
    mailbox->sequence = sequence;
    atomic_store_explicit(&mailbox->state, ARAM_DATA_READY, memory_order_release);
}

void aram_data_cancel_write(aram_data_mailbox_t *mailbox) {
    if (mailbox != NULL) {
        atomic_store_explicit(&mailbox->state, ARAM_DATA_FREE, memory_order_release);
    }
}

bool aram_data_acquire(aram_data_mailbox_t *mailbox, aram_data_snapshot_t *snapshot) {
    if (mailbox == NULL || snapshot == NULL || snapshot->held)
        return false;
    uint32_t expected = ARAM_DATA_READY;
    if (!atomic_compare_exchange_strong_explicit(&mailbox->state, &expected, ARAM_DATA_READING,
                                                 memory_order_acq_rel, memory_order_acquire)) {
        return false;
    }
    *snapshot = (aram_data_snapshot_t){
        .pixels = mailbox->pixels,
        .request = mailbox->request,
        .generation = mailbox->generation,
        .sequence = mailbox->sequence,
        .held = true,
    };
    return true;
}

void aram_data_release(aram_data_mailbox_t *mailbox, aram_data_snapshot_t *snapshot) {
    if (mailbox == NULL || snapshot == NULL || !snapshot->held)
        return;
    atomic_store_explicit(&mailbox->state, ARAM_DATA_FREE, memory_order_release);
    *snapshot = (aram_data_snapshot_t){0};
}
