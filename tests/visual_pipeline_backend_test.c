#include "app/visual_pipeline.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void assert_empty(const uint8_t *bitmap) {
    for (size_t i = 0u; i < ARAM_ACTIVITY_BITMAP_BYTES; ++i)
        assert(bitmap[i] == 0u);
}

static void render(software_spc_backend_t *backend) {
    int16_t samples[PCM_BLOCK_FRAMES * 2u];
    assert(software_spc_render(backend, samples, PCM_BLOCK_FRAMES) == NULL);
}

int main(void) {
    static uint8_t spc[0x10180];
    memcpy(spc, "SNES-SPC700 Sound File Data v0.30", 31u);
    spc[0x21] = 0x1au;
    spc[0x22] = 0x1au;
    spc[0x23] = 0x1au;
    spc[0x24] = 30u;
    spc[0x26] = 2u; /* PC = $0200. INC $30; BRA back to INC. */
    spc[0x100 + 0x200] = 0xabu;
    spc[0x100 + 0x201] = 0x30u;
    spc[0x100 + 0x202] = 0x2fu;
    spc[0x100 + 0x203] = 0xfcu;
    spc[0x10100 + 0x6d] = 0x80u; /* Echo buffer at $8000, writes enabled. */
    spc[0x10100 + 0x7d] = 1u;

    software_spc_backend_t *backend = software_spc_create();
    assert(backend != NULL);
    assert(software_spc_load(backend, spc, sizeof(spc), true) == NULL);
    player_t player = {.backend = backend, .mode = PLAYER_PLAYING,
                       .ready = true, .has_track = true, .generation = 7u};
    static visual_pipeline_t pipeline;
    static pcm_queue_t queue;
    visual_pipeline_init(&pipeline, 60u);
    pcm_queue_init(&queue);
    visual_pipeline_bind_backend(&pipeline, backend);
    visual_pipeline_service(&pipeline, &player, &queue, false);
    aram_activity_snapshot_t held = {0};
    assert(aram_activity_acquire(&pipeline.activity, &held));
    aram_activity_release(&pipeline.activity, &held);

    uint8_t *read;
    uint8_t *write;
    uint8_t *execute;
    aram_activity_producer_maps(&pipeline.activity, &read, &write, &execute);
    render(backend);
    assert(aram_activity_test_bit(read, 0x30u));
    assert(aram_activity_test_bit(write, 0x30u));
    assert(aram_activity_test_bit(execute, 0x200u));
    assert(aram_activity_test_bit(write, 0x8000u));

    /* A consumer-held bank must survive a mode switch unchanged. */
    assert(aram_activity_publish(&pipeline.activity));
    assert(aram_activity_acquire(&pipeline.activity, &held));
    visual_pipeline_bind_backend(&pipeline, backend);
    aram_activity_producer_maps(&pipeline.activity, &read, &write, &execute);
    aram_activity_mark(write, 0x1234u);
    visual_pipeline_set_aram_request(&pipeline, 1u);
    visual_pipeline_service(&pipeline, &player, &queue, false);
    assert_empty(read);
    assert_empty(write);
    assert_empty(execute);
    render(backend);
    assert_empty(read);
    assert_empty(write);
    assert_empty(execute);
    assert(aram_activity_test_bit(held.write, 0x30u));
    assert(aram_activity_test_bit(held.write, 0x8000u));
    aram_activity_release(&pipeline.activity, &held);

    /* A track generation change in Data must keep recording disabled. */
    ++player.generation;
    visual_pipeline_service(&pipeline, &player, &queue, false);
    render(backend);
    aram_activity_producer_maps(&pipeline.activity, &read, &write, &execute);
    assert_empty(read);
    assert_empty(write);
    assert_empty(execute);

    visual_pipeline_set_aram_request(&pipeline, 2u);
    visual_pipeline_service(&pipeline, &player, &queue, false);
    assert(aram_activity_acquire(&pipeline.activity, &held));
    assert(held.generation == 8u);
    assert_empty(held.read);
    assert_empty(held.write);
    assert_empty(held.execute);
    render(backend);
    aram_activity_producer_maps(&pipeline.activity, &read, &write, &execute);
    assert(aram_activity_test_bit(write, 0x30u));
    assert(aram_activity_test_bit(execute, 0x200u));
    assert_empty(held.write);
    aram_activity_release(&pipeline.activity, &held);

    /* An unread publication is discarded when entering Data. */
    assert(aram_activity_publish(&pipeline.activity));
    visual_pipeline_bind_backend(&pipeline, backend);
    visual_pipeline_set_aram_request(&pipeline, 3u);
    visual_pipeline_service(&pipeline, &player, &queue, false);
    assert(!aram_activity_acquire(&pipeline.activity, &held));

    /* Latest-value requests can skip an intervening Data mode. The new
     * Activity token still starts fresh, including unread old publications. */
    visual_pipeline_set_aram_request(&pipeline, 4u);
    visual_pipeline_service(&pipeline, &player, &queue, false);
    assert(aram_activity_acquire(&pipeline.activity, &held));
    aram_activity_release(&pipeline.activity, &held);
    render(backend);
    assert(aram_activity_publish(&pipeline.activity));
    visual_pipeline_bind_backend(&pipeline, backend);
    aram_activity_producer_maps(&pipeline.activity, &read, &write, &execute);
    aram_activity_mark(write, 0x1234u);
    visual_pipeline_set_aram_request(&pipeline, 5u);
    visual_pipeline_set_aram_request(&pipeline, 6u);
    visual_pipeline_service(&pipeline, &player, &queue, false);
    assert(aram_activity_acquire(&pipeline.activity, &held));
    assert_empty(held.read);
    assert_empty(held.write);
    assert_empty(held.execute);
    render(backend);
    aram_activity_producer_maps(&pipeline.activity, &read, &write, &execute);
    assert(aram_activity_test_bit(write, 0x30u));
    assert(!aram_activity_test_bit(write, 0x1234u));
    aram_activity_release(&pipeline.activity, &held);
    software_spc_destroy(backend);
    puts("Data/Activity recording transitions with real CPU and echo writes OK");
    return 0;
}
