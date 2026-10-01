#include "app/visual_consumer.h"
#if PICO_SPC_UART_PERFORMANCE
#include "pico/time.h"
#endif

void visual_consumer_init(visual_consumer_t *consumer) {
    *consumer = (visual_consumer_t){0};
}

bool visual_consumer_poll(visual_consumer_t *consumer, aram_activity_t *activity,
                          aram_data_mailbox_t *data, aram_view_t *view,
                          uint32_t generation, uint32_t request, bool map_visible) {
    bool changed = false;
    aram_activity_snapshot_t snapshot = {0};
    if (aram_activity_acquire(activity, &snapshot)) {
        if ((request & 1u) == 0u && snapshot.request == request &&
            snapshot.generation == generation) {
#if PICO_SPC_UART_PERFORMANCE
            const uint32_t started_us = time_us_32();
#endif
            aram_view_apply(view, &snapshot);
#if PICO_SPC_UART_PERFORMANCE
            const uint32_t elapsed_us = time_us_32() - started_us;
            if (elapsed_us > consumer->activity_max_us) consumer->activity_max_us = elapsed_us;
#endif
            changed = true;
        } else {
            ++consumer->discarded;
        }
        aram_activity_release(activity, &snapshot);
    }
    aram_data_snapshot_t data_snapshot = {0};
    if (aram_data_acquire(data, &data_snapshot)) {
        if (map_visible && (request & 1u) != 0u && data_snapshot.request == request &&
            data_snapshot.generation == generation) {
            aram_view_apply_data(view, &data_snapshot);
            changed = true;
        } else {
            ++consumer->discarded;
        }
        aram_data_release(data, &data_snapshot);
    }
    if (changed) {
        view->sequence = ++consumer->revision;
        ++consumer->consumed;
    }
    return changed;
}
