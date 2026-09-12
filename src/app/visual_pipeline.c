#include "app/visual_pipeline.h"

#include <limits.h>
#include <stddef.h>

void visual_pipeline_init(visual_pipeline_t *pipeline, uint8_t initial_hz) {
    if (pipeline == NULL) {
        return;
    }
    spc_snapshot_queue_init(&pipeline->snapshots);
    aram_activity_init(&pipeline->activity, 0u);
    aram_data_init(&pipeline->data);
    pipeline->rate.requested_hz = initial_hz;
    pipeline->rate.phase = 0u;
    atomic_init(&pipeline->requested_hz, initial_hz);
    atomic_init(&pipeline->requested_aram_map, 0u);
    pipeline->snapshot_sequence = 0u;
    pipeline->snapshot_drops = 0u;
    pipeline->snapshot_generation = 0u;
    pipeline->data_sequence = 0u;
    pipeline->published_map_request = 0u;
    pipeline->published_map_generation = UINT32_MAX;
}

void visual_pipeline_bind_backend(visual_pipeline_t *pipeline, software_spc_backend_t *backend) {
    if (pipeline == NULL || backend == NULL) {
        return;
    }
    uint8_t *read;
    uint8_t *write;
    uint8_t *execute;
    aram_activity_producer_maps(&pipeline->activity, &read, &write, &execute);
    software_spc_set_aram_visualizer(backend, read, write, execute);
}

uint8_t visual_pipeline_requested_hz(const visual_pipeline_t *pipeline) {
    const uint32_t requested = atomic_load_explicit(&pipeline->requested_hz, memory_order_acquire);
    return requested == 60u ? 60u : 30u;
}

void visual_pipeline_set_requested_hz(visual_pipeline_t *pipeline, uint8_t requested_hz) {
    atomic_store_explicit(&pipeline->requested_hz, requested_hz == 60u ? 60u : 30u,
                          memory_order_release);
}

void visual_pipeline_set_aram_request(visual_pipeline_t *pipeline, uint32_t request) {
    atomic_store_explicit(&pipeline->requested_aram_map, request, memory_order_release);
}

void visual_pipeline_service(visual_pipeline_t *pipeline, player_t *player,
                             const pcm_queue_t *pcm_queue, bool allow_scheduled) {
    if (pipeline == NULL || player == NULL || pcm_queue == NULL || !player->ready ||
        !player->has_track || player->backend == NULL) {
        return;
    }

    if (player->generation != pipeline->snapshot_generation) {
        pipeline->snapshot_generation = player->generation;
        pipeline->rate.phase = 0u;
        aram_activity_reset_generation(&pipeline->activity, player->generation);
        visual_pipeline_bind_backend(pipeline, player->backend);
    }

    const uint32_t request =
        atomic_load_explicit(&pipeline->requested_aram_map, memory_order_acquire);
    const bool data_mode = (request & 1u) != 0u;
    const bool forced = request != pipeline->published_map_request ||
                        player->generation != pipeline->published_map_generation;
    const bool paused = player->mode == PLAYER_PAUSED || player->mode == PLAYER_FADING_FOR_PAUSE;
    if (paused) {
        pipeline->rate.phase = 0u;
    }
    visual_rate_set(&pipeline->rate, visual_pipeline_requested_hz(pipeline));
    const bool scheduled =
        allow_scheduled && !paused && visual_rate_advance(&pipeline->rate, PCM_BLOCK_FRAMES);
    if (!forced && !scheduled) {
        return;
    }

    bool map_published = false;
    if (data_mode) {
        /* A heatmap copies 32 KiB, so audio retains two complete queued blocks first. */
        const size_t queued_frames = PCM_QUEUE_CAPACITY - pcm_queue_space(pcm_queue);
        if (queued_frames >= PCM_BLOCK_FRAMES * 2u) {
            uint8_t *pixels = aram_data_begin_write(&pipeline->data);
            if (pixels != NULL) {
                if (software_spc_capture_aram_heatmap(player->backend, pixels)) {
                    aram_data_publish(&pipeline->data, request, player->generation,
                                      ++pipeline->data_sequence);
                    map_published = true;
                } else {
                    aram_data_cancel_write(&pipeline->data);
                }
            }
        }
    } else if (aram_activity_publish(&pipeline->activity)) {
        visual_pipeline_bind_backend(pipeline, player->backend);
        map_published = true;
    }

    if (map_published) {
        pipeline->published_map_request = request;
        pipeline->published_map_generation = player->generation;
    }
    if (data_mode && !scheduled && !map_published) {
        return;
    }

    spc_snapshot_t snapshot;
    ++pipeline->snapshot_sequence;
    if (!software_spc_capture_snapshot(player->backend, player->generation,
                                       pipeline->snapshot_sequence, pipeline->snapshot_drops,
                                       &snapshot)) {
        return;
    }
    if (!spc_snapshot_queue_push(&pipeline->snapshots, &snapshot)) {
        ++pipeline->snapshot_drops;
    }
}
