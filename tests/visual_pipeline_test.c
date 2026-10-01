#include "app/visual_pipeline.h"
#include "app/visual_consumer.h"

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
    const visual_pipeline_stats_t stats = visual_pipeline_get_stats(&pipeline);
    assert(stats.scheduled == 0u && stats.published == 0u && stats.headroom == 0u);
}

static visual_pipeline_t timed_pipeline;
static aram_view_t timed_view;

static void test_polling_cadence(uint8_t hz, bool data_mode, unsigned poll_phase) {
    struct software_spc_backend backend = {0};
    player_t player = ready_player(&backend);
    pcm_queue_t pcm_queue;
    pcm_queue_init(&pcm_queue);
    queue_audio_headroom(&pcm_queue);
    visual_pipeline_init(&timed_pipeline, hz);
    visual_pipeline_set_aram_request(&timed_pipeline, data_mode ? 1u : 0u);
    aram_view_init(&timed_view);
    visual_consumer_t consumer;
    visual_consumer_init(&consumer);
    uint32_t previous_revision = 0u;
    unsigned dirty_events = 0u;
    /* Real producer advances once per 8ms audio block. Consumer polls every
     * 7ms with several offsets, independently of either 30/60Hz model phase.
     * Repeated polls must not queue duplicate maps or replay missed deadlines. */
    for (unsigned ms = 1u; ms <= 1007u; ++ms) {
        if (ms <= 1000u && ms % 8u == 0u) {
            visual_pipeline_service(&timed_pipeline, &player, &pcm_queue, true);
            spc_snapshot_t snapshot;
            (void)spc_snapshot_queue_pop_latest(&timed_pipeline.snapshots, &snapshot);
        }
        if (ms % 7u == poll_phase) {
            const bool changed = visual_consumer_poll(&consumer, &timed_pipeline.activity,
                &timed_pipeline.data, &timed_view, player.generation,
                data_mode ? 1u : 0u, true);
            if (changed) {
                ++dirty_events;
                assert(timed_view.sequence == previous_revision + 1u);
                previous_revision = timed_view.sequence;
            }
        }
    }
    const visual_pipeline_stats_t stats = visual_pipeline_get_stats(&timed_pipeline);
    assert(stats.scheduled == hz);
    assert(stats.published == (uint32_t)hz + 1u); /* Initial forced refresh. */
    assert(stats.busy == 0u && stats.headroom == 0u);
    assert(consumer.consumed == stats.published && dirty_events == stats.published);
    assert(consumer.discarded == 0u);
}

static void test_delayed_consumer_and_headroom(void) {
    struct software_spc_backend backend = {0};
    player_t player = ready_player(&backend);
    pcm_queue_t pcm_queue;
    pcm_queue_init(&pcm_queue);
    queue_audio_headroom(&pcm_queue);
    visual_pipeline_init(&timed_pipeline, 60u);
    visual_pipeline_set_aram_request(&timed_pipeline, 1u);
    visual_pipeline_service(&timed_pipeline, &player, &pcm_queue, true);
    aram_data_snapshot_t held = {0};
    assert(aram_data_acquire(&timed_pipeline.data, &held));
    /* Long display/render delay retains ownership, without producer waiting. */
    for (unsigned block = 1u; block < 125u; ++block)
        visual_pipeline_service(&timed_pipeline, &player, &pcm_queue, true);
    visual_pipeline_stats_t stats = visual_pipeline_get_stats(&timed_pipeline);
    assert(stats.scheduled == 60u && stats.published == 1u && stats.busy == 60u);
    assert(held.sequence == 1u && held.pixels[0] == 0u);
    aram_data_release(&timed_pipeline.data, &held);
    pcm_queue_init(&pcm_queue);
    /* Scheduled requests retain the original two-block headroom guard. Forced
     * retries with no new audio block cannot flood these diagnostic totals. */
    for (unsigned block = 0u; block < 125u; ++block)
        visual_pipeline_service(&timed_pipeline, &player, &pcm_queue, true);
    visual_pipeline_set_aram_request(&timed_pipeline, 3u);
    for (unsigned retry = 0u; retry < 1000u; ++retry)
        visual_pipeline_service(&timed_pipeline, &player, &pcm_queue, false);
    stats = visual_pipeline_get_stats(&timed_pipeline);
    assert(stats.scheduled == 120u && stats.headroom == 60u && stats.published == 1u);
    player.mode = PLAYER_PAUSED;
    for (unsigned block = 0u; block < 125u; ++block)
        visual_pipeline_service(&timed_pipeline, &player, &pcm_queue, true);
    assert(visual_pipeline_get_stats(&timed_pipeline).scheduled == 120u);
}

int main(void) {
    test_data_mode_keeps_scheduled_voice_snapshots();
    test_failed_forced_heatmap_does_not_flood_snapshots();
    for (unsigned phase = 0u; phase < 7u; ++phase) {
        test_polling_cadence(30u, false, phase);
        test_polling_cadence(60u, false, phase);
        test_polling_cadence(30u, true, phase);
        test_polling_cadence(60u, true, phase);
    }
    test_delayed_consumer_and_headroom();
    puts("visual pipeline contract OK");
    return 0;
}
