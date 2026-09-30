#ifndef APP_VISUAL_PIPELINE_H
#define APP_VISUAL_PIPELINE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#include "audio/pcm_queue.h"
#include "player/player.h"
#include "spc/spc_snapshot_queue.h"
#include "visualizer/aram_activity.h"
#include "visualizer/aram_data.h"
#include "visualizer/visual_rate.h"

/* Core 0 owns producer-side state. Core 1 changes requests atomically and
 * consumes copied queue/mailbox snapshots; it never reads live emulator RAM. */
typedef struct {
    spc_snapshot_queue_t snapshots;
    aram_activity_t activity;
    aram_data_mailbox_t data;
    visual_rate_t rate;
    _Atomic uint32_t requested_hz;
    _Atomic uint32_t requested_aram_map;
    uint32_t snapshot_sequence;
    uint32_t snapshot_drops;
    uint32_t snapshot_generation;
    uint32_t data_sequence;
    uint32_t published_map_request;
    uint32_t published_map_generation;
    uint32_t processed_aram_request;
    bool activity_recording;
} visual_pipeline_t;

/* Initializes all producer/consumer state. This function does not block. */
void visual_pipeline_init(visual_pipeline_t *pipeline, uint8_t initial_hz);

/* Binds activity maps, or disables recording in Data mode, on Core 0. */
void visual_pipeline_bind_backend(visual_pipeline_t *pipeline, software_spc_backend_t *backend);

/* Opportunistically publishes copied visual state on Core 0. This function does not block. */
void visual_pipeline_service(visual_pipeline_t *pipeline, player_t *player,
                             const pcm_queue_t *pcm_queue, bool allow_scheduled);

/* Reads or updates the requested publication frequency in hertz without blocking. */
uint8_t visual_pipeline_requested_hz(const visual_pipeline_t *pipeline);
void visual_pipeline_set_requested_hz(visual_pipeline_t *pipeline, uint8_t requested_hz);

/* Updates the opaque ARAM mode request token without blocking. Bit zero selects Data mode. */
void visual_pipeline_set_aram_request(visual_pipeline_t *pipeline, uint32_t request);

#endif
