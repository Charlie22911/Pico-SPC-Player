#include "aram_activity.h"

#include <stddef.h>
#include <string.h>

static void clear_bank(aram_activity_bank_t *bank) {
    memset(bank->read, 0, sizeof(bank->read));
    memset(bank->write, 0, sizeof(bank->write));
    memset(bank->execute, 0, sizeof(bank->execute));
}

void aram_activity_init(aram_activity_t *activity, uint32_t generation) {
    if (activity == NULL)
        return;
    memset(activity, 0, sizeof(*activity));
    atomic_init(&activity->states[0], ARAM_ACTIVITY_BANK_WRITING);
    atomic_init(&activity->states[1], ARAM_ACTIVITY_BANK_FREE);
    activity->generation = generation;
    activity->banks[0].generation = generation;
}

void aram_activity_reset_generation(aram_activity_t *activity, uint32_t generation) {
    if (activity == NULL || activity->generation == generation)
        return;
    activity->generation = generation;
    aram_activity_bank_t *producer = &activity->banks[activity->producer_bank];
    clear_bank(producer);
    producer->generation = generation;

    const uint32_t other = activity->producer_bank ^ 1u;
    uint32_t expected = ARAM_ACTIVITY_BANK_READY;
    (void)atomic_compare_exchange_strong_explicit(&activity->states[other], &expected,
                                                  ARAM_ACTIVITY_BANK_FREE, memory_order_acq_rel,
                                                  memory_order_acquire);
}

void aram_activity_producer_maps(aram_activity_t *activity, uint8_t **read, uint8_t **write,
                                 uint8_t **execute) {
    aram_activity_bank_t *bank = &activity->banks[activity->producer_bank];
    if (read != NULL)
        *read = bank->read;
    if (write != NULL)
        *write = bank->write;
    if (execute != NULL)
        *execute = bank->execute;
}

bool aram_activity_publish(aram_activity_t *activity) {
    if (activity == NULL)
        return false;
    const uint32_t current = activity->producer_bank;
    const uint32_t next = current ^ 1u;
    if (atomic_load_explicit(&activity->states[next], memory_order_acquire) !=
        ARAM_ACTIVITY_BANK_FREE) {
        return false;
    }

    clear_bank(&activity->banks[next]);
    activity->banks[next].generation = activity->generation;
    atomic_store_explicit(&activity->states[next], ARAM_ACTIVITY_BANK_WRITING,
                          memory_order_relaxed);
    activity->banks[current].generation = activity->generation;
    activity->banks[current].sequence = ++activity->sequence;
    activity->producer_bank = next;
    atomic_store_explicit(&activity->states[current], ARAM_ACTIVITY_BANK_READY,
                          memory_order_release);
    return true;
}

bool aram_activity_acquire(aram_activity_t *activity, aram_activity_snapshot_t *snapshot) {
    if (activity == NULL || snapshot == NULL || snapshot->held)
        return false;
    for (uint32_t bank = 0u; bank < ARAM_ACTIVITY_BANK_COUNT; ++bank) {
        uint32_t expected = ARAM_ACTIVITY_BANK_READY;
        if (atomic_compare_exchange_strong_explicit(&activity->states[bank], &expected,
                                                    ARAM_ACTIVITY_BANK_READING,
                                                    memory_order_acq_rel, memory_order_acquire)) {
            const aram_activity_bank_t *source = &activity->banks[bank];
            *snapshot = (aram_activity_snapshot_t){
                .read = source->read,
                .write = source->write,
                .execute = source->execute,
                .generation = source->generation,
                .sequence = source->sequence,
                .bank = bank,
                .held = true,
            };
            return true;
        }
    }
    return false;
}

void aram_activity_release(aram_activity_t *activity, aram_activity_snapshot_t *snapshot) {
    if (activity == NULL || snapshot == NULL || !snapshot->held ||
        snapshot->bank >= ARAM_ACTIVITY_BANK_COUNT) {
        return;
    }
    atomic_store_explicit(&activity->states[snapshot->bank], ARAM_ACTIVITY_BANK_FREE,
                          memory_order_release);
    *snapshot = (aram_activity_snapshot_t){0};
}
