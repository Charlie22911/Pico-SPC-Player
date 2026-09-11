#include "storage_mailbox.h"

void storage_mailbox_init(storage_mailbox_t *mailbox) {
    if (mailbox == NULL)
        return;
    atomic_init(&mailbox->state, STORAGE_MAILBOX_EMPTY);
    mailbox->size = 0u;
    mailbox->track = (spc_track_ref_t){0};
    mailbox->accepted = false;
    mailbox->generation = 0u;
}

uint8_t *storage_mailbox_begin_write(storage_mailbox_t *mailbox) {
    if (mailbox == NULL)
        return NULL;
    uint32_t expected = STORAGE_MAILBOX_EMPTY;
    if (!atomic_compare_exchange_strong_explicit(&mailbox->state, &expected,
                                                 STORAGE_MAILBOX_WRITING, memory_order_acquire,
                                                 memory_order_relaxed)) {
        return NULL;
    }
    return mailbox->image;
}

void storage_mailbox_cancel_write(storage_mailbox_t *mailbox) {
    if (mailbox != NULL) {
        atomic_store_explicit(&mailbox->state, STORAGE_MAILBOX_EMPTY, memory_order_release);
    }
}

void storage_mailbox_publish(storage_mailbox_t *mailbox, size_t size, spc_track_ref_t track) {
    if (mailbox == NULL || size > sizeof(mailbox->image))
        return;
    mailbox->size = size;
    mailbox->track = track;
    atomic_store_explicit(&mailbox->state, STORAGE_MAILBOX_READY, memory_order_release);
}

bool storage_mailbox_begin_read(storage_mailbox_t *mailbox, const uint8_t **image, size_t *size,
                                spc_track_ref_t *track) {
    if (mailbox == NULL || image == NULL || size == NULL || track == NULL)
        return false;
    uint32_t expected = STORAGE_MAILBOX_READY;
    if (!atomic_compare_exchange_strong_explicit(&mailbox->state, &expected,
                                                 STORAGE_MAILBOX_READING, memory_order_acquire,
                                                 memory_order_relaxed)) {
        return false;
    }
    *image = mailbox->image;
    *size = mailbox->size;
    *track = mailbox->track;
    return true;
}

void storage_mailbox_complete_read(storage_mailbox_t *mailbox, bool accepted, uint32_t generation) {
    if (mailbox == NULL)
        return;
    mailbox->accepted = accepted;
    mailbox->generation = generation;
    atomic_store_explicit(&mailbox->state, STORAGE_MAILBOX_COMPLETE, memory_order_release);
}

bool storage_mailbox_reclaim(storage_mailbox_t *mailbox, bool *accepted, uint32_t *generation) {
    if (mailbox == NULL || accepted == NULL || generation == NULL)
        return false;
    uint32_t expected = STORAGE_MAILBOX_COMPLETE;
    if (!atomic_compare_exchange_strong_explicit(&mailbox->state, &expected, STORAGE_MAILBOX_EMPTY,
                                                 memory_order_acquire, memory_order_relaxed)) {
        return false;
    }
    *accepted = mailbox->accepted;
    *generation = mailbox->generation;
    return true;
}
