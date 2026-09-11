#include "pcm_queue.h"
#include <string.h>
_Static_assert(ATOMIC_INT_LOCK_FREE == 2 && sizeof(unsigned int) == 4,
               "Audio requires lock-free 32-bit atomics");
_Static_assert((PCM_QUEUE_CAPACITY & (PCM_QUEUE_CAPACITY - 1u)) == 0,
               "Ring size must be a power of two");
_Static_assert(PCM_QUEUE_CAPACITY % PCM_BLOCK_FRAMES == 0u,
               "Audio queue must contain whole render blocks");
_Static_assert((PCM_QUEUE_BLOCK_CAPACITY & (PCM_QUEUE_BLOCK_CAPACITY - 1u)) == 0u,
               "Block ring size must be a power of two");
void pcm_queue_init(pcm_queue_t *queue) {
    atomic_init(&queue->head, 0u);
    atomic_init(&queue->tail, 0u);
}
/* Call available from the consumer and space from the producer. Other
 * observers use separately published diagnostics. */
size_t pcm_queue_available(const pcm_queue_t *q) {
    uint32_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&q->head, memory_order_acquire);
    return (size_t)(uint32_t)(head - tail) * PCM_BLOCK_FRAMES;
}
size_t pcm_queue_space(const pcm_queue_t *q) {
    uint32_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    return (PCM_QUEUE_BLOCK_CAPACITY - (uint32_t)(head - tail)) * PCM_BLOCK_FRAMES;
}

bool pcm_queue_write_block(pcm_queue_t *q, const uint32_t *frames, const pcm_block_info_t *info) {
    if (q == NULL || frames == NULL || info == NULL)
        return false;
    const uint32_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
    const uint32_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if ((uint32_t)(head - tail) == PCM_QUEUE_BLOCK_CAPACITY)
        return false;
    const size_t block_index = head & (PCM_QUEUE_BLOCK_CAPACITY - 1u);
    memcpy(q->blocks[block_index].frames, frames, PCM_BLOCK_FRAMES * sizeof(*frames));
    q->blocks[block_index].info = *info;
    atomic_store_explicit(&q->head, head + 1u, memory_order_release);
    return true;
}

bool pcm_queue_read_block(pcm_queue_t *q, uint32_t *frames, pcm_block_info_t *info) {
    if (q == NULL || frames == NULL || info == NULL)
        return false;
    const uint32_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    const uint32_t head = atomic_load_explicit(&q->head, memory_order_acquire);
    if (tail == head)
        return false;
    const size_t block_index = tail & (PCM_QUEUE_BLOCK_CAPACITY - 1u);
    memcpy(frames, q->blocks[block_index].frames, PCM_BLOCK_FRAMES * sizeof(*frames));
    *info = q->blocks[block_index].info;
    atomic_store_explicit(&q->tail, tail + 1u, memory_order_release);
    return true;
}
