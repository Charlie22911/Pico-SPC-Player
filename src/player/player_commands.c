#include "player_commands.h"

#include <stddef.h>

_Static_assert((PLAYER_COMMAND_CAPACITY & (PLAYER_COMMAND_CAPACITY - 1u)) == 0u,
               "Command capacity must be a power of two");

void player_command_queue_init(player_command_queue_t *queue) {
    atomic_init(&queue->head, 0u);
    atomic_init(&queue->tail, 0u);
}

bool player_command_push(player_command_queue_t *queue, player_command_t command) {
    const uint32_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    const uint32_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    if ((uint32_t)(head - tail) == PLAYER_COMMAND_CAPACITY)
        return false;
    queue->entries[head & (PLAYER_COMMAND_CAPACITY - 1u)] = command;
    atomic_store_explicit(&queue->head, head + 1u, memory_order_release);
    return true;
}

bool player_command_pop(player_command_queue_t *queue, player_command_t *command) {
    const uint32_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    const uint32_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    if (tail == head || command == NULL)
        return false;
    *command = queue->entries[tail & (PLAYER_COMMAND_CAPACITY - 1u)];
    atomic_store_explicit(&queue->tail, tail + 1u, memory_order_release);
    return true;
}
