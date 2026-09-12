#include "app/visual_pipeline.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct software_spc_backend {
    uint8_t unused;
};

void software_spc_set_aram_visualizer(software_spc_backend_t *backend, uint8_t *read_bitmap,
                                      uint8_t *write_bitmap, uint8_t *execute_bitmap) {
    (void)backend;
    (void)read_bitmap;
    (void)write_bitmap;
    (void)execute_bitmap;
}

bool software_spc_capture_aram_heatmap(const software_spc_backend_t *backend,
                                       uint8_t packed[ARAM_DATA_PIXELS_BYTES]) {
    (void)backend;
    memset(packed, 0, ARAM_DATA_PIXELS_BYTES);
    return true;
}

bool software_spc_capture_snapshot(const software_spc_backend_t *backend, uint32_t generation,
                                   uint32_t sequence, uint32_t dropped_publications,
                                   spc_snapshot_t *snapshot) {
    (void)backend;
    *snapshot = (spc_snapshot_t){
        .sequence = sequence,
        .generation = generation,
        .dropped_publications = dropped_publications,
    };
    return true;
}

static player_t ready_player(software_spc_backend_t *backend) {
    return (player_t){
        .backend = backend,
        .mode = PLAYER_PLAYING,
        .ready = true,
        .has_track = true,
        .generation = 7u,
    };
}

static void queue_audio_headroom(pcm_queue_t *queue) {
    const uint32_t frames[PCM_BLOCK_FRAMES] = {0};
    const pcm_block_info_t info = {0};
    assert(pcm_queue_write_block(queue, frames, &info));
    assert(pcm_queue_write_block(queue, frames, &info));
}

static void test_data_mode_keeps_scheduled_voice_snapshots(void) {
    struct software_spc_backend backend = {0};
    player_t player = ready_player(&backend);
    pcm_queue_t pcm_queue;
    pcm_queue_init(&pcm_queue);
    queue_audio_headroom(&pcm_queue);

    visual_pipeline_t pipeline;
    visual_pipeline_init(&pipeline, 60u);
    visual_pipeline_set_aram_request(&pipeline, 1u);

    visual_pipeline_service(&pipeline, &player, &pcm_queue, true);
    visual_pipeline_service(&pipeline, &player, &pcm_queue, true);
    visual_pipeline_service(&pipeline, &player, &pcm_queue, true);

    spc_snapshot_t snapshot = {0};
    assert(spc_snapshot_queue_pop_latest(&pipeline.snapshots, &snapshot));
    assert(snapshot.sequence == 2u);
    assert(snapshot.generation == 7u);
}

static void test_failed_forced_heatmap_does_not_flood_snapshots(void) {
    struct software_spc_backend backend = {0};
    player_t player = ready_player(&backend);
    pcm_queue_t pcm_queue;
    pcm_queue_init(&pcm_queue);

    visual_pipeline_t pipeline;
    visual_pipeline_init(&pipeline, 60u);
    visual_pipeline_set_aram_request(&pipeline, 1u);
    visual_pipeline_service(&pipeline, &player, &pcm_queue, false);

    spc_snapshot_t snapshot = {0};
    assert(!spc_snapshot_queue_pop_latest(&pipeline.snapshots, &snapshot));
}

int main(void) {
    test_data_mode_keeps_scheduled_voice_snapshots();
    test_failed_forced_heatmap_does_not_flood_snapshots();
    puts("visual pipeline contract OK");
    return 0;
}
