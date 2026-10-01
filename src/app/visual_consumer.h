#ifndef APP_VISUAL_CONSUMER_H
#define APP_VISUAL_CONSUMER_H

#include "visualizer/aram_activity.h"
#include "visualizer/aram_data.h"
#include "visualizer/aram_view.h"

/* Core 1 owns these counters and the copied view. No live emulator reads. */
typedef struct {
    uint32_t revision;
    uint32_t consumed;
    uint32_t discarded;
    uint32_t activity_max_us;
} visual_consumer_t;

void visual_consumer_init(visual_consumer_t *consumer);
/* Poll once per UI loop, independently of the ordinary model timer. Returns
 * true only for a newly accepted map. Always releases acquired snapshots.
 * Activity continues fading off-screen; Data copies are needed only on-screen. */
bool visual_consumer_poll(visual_consumer_t *consumer, aram_activity_t *activity,
                          aram_data_mailbox_t *data, aram_view_t *view,
                          uint32_t generation, uint32_t request, bool map_visible);
#endif
