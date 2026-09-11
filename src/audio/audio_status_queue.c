#include "audio_status_queue.h"

#include <stddef.h>

_Static_assert(ATOMIC_INT_LOCK_FREE == 2 && sizeof(unsigned int) == 4,
               "Status queue requires lock-free 32-bit atomics");
_Static_assert((AUDIO_STATUS_QUEUE_CAPACITY & (AUDIO_STATUS_QUEUE_CAPACITY - 1u)) == 0u,
               "Status queue capacity must be a power of two");

void audio_status_queue_init(audio_status_queue_t *queue) {
    atomic_init(&queue->head, 0u);
    atomic_init(&queue->tail, 0u);
}

bool audio_status_queue_push(audio_status_queue_t *queue, const audio_status_snapshot_t *snapshot) {
    if (queue == NULL || snapshot == NULL)
        return false;
    const uint32_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    const uint32_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    if ((uint32_t)(head - tail) == AUDIO_STATUS_QUEUE_CAPACITY)
        return false;
    queue->entries[head & (AUDIO_STATUS_QUEUE_CAPACITY - 1u)] = *snapshot;
    atomic_store_explicit(&queue->head, head + 1u, memory_order_release);
    return true;
}

bool audio_status_queue_pop(audio_status_queue_t *queue, audio_status_snapshot_t *snapshot) {
    if (queue == NULL || snapshot == NULL)
        return false;
    const uint32_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    const uint32_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    if (tail == head)
        return false;
    *snapshot = queue->entries[tail & (AUDIO_STATUS_QUEUE_CAPACITY - 1u)];
    atomic_store_explicit(&queue->tail, tail + 1u, memory_order_release);
    return true;
}

bool audio_status_queue_pop_latest(audio_status_queue_t *queue, audio_status_snapshot_t *snapshot) {
    if (queue == NULL || snapshot == NULL)
        return false;
    const uint32_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    const uint32_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    if (tail == head)
        return false;
    *snapshot = queue->entries[(head - 1u) & (AUDIO_STATUS_QUEUE_CAPACITY - 1u)];
    atomic_store_explicit(&queue->tail, head, memory_order_release);
    return true;
}
