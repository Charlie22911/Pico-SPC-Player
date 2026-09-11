#include "audio/pcm_queue.h"
#include "audio/pcm_format.h"
#include "audio/audio_status_queue.h"
#include "audio/test_tone.h"
#include "visualizer/visual_rate.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void test_packing(void) {
    assert(pcm_pack_i2s(0x1234, (int16_t)-292) == UINT32_C(0x1234fedc));
    assert(pcm_pack_i2s(INT16_MIN, INT16_MAX) == UINT32_C(0x80007fff));
    assert(pcm_pack_i2s(-1, 0) == UINT32_C(0xffff0000));
    assert(pcm_peak_magnitude(INT16_MIN) == 32768u);
    assert(pcm_peak_magnitude(INT16_MAX) == 32767u);
    assert(pcm_peak_magnitude(-1) == 1u);
}

static void test_clocks(void) {
    uint32_t divider = 0;
    assert(pcm_i2s_divider(250000000u, 32000u, &divider));
    assert(divider == 31250u);
    assert(pcm_i2s_divider(150000000u, 32000u, &divider));
    assert(divider == 18750u);
    assert(!pcm_i2s_divider(250000000u, 0, &divider));
    assert(!pcm_i2s_divider(1, 32000u, &divider));
    assert(!pcm_i2s_divider(UINT32_MAX, 1, &divider));
    assert(!pcm_i2s_divider(250000000u, 32000u, NULL));
}

static void test_queue(void) {
    pcm_queue_t q;
    uint32_t in[PCM_BLOCK_FRAMES], out[PCM_BLOCK_FRAMES];
    for (uint32_t i = 0; i < PCM_BLOCK_FRAMES; ++i)
        in[i] = i + 17u;
    pcm_queue_init(&q);
    const pcm_block_info_t block_in = {
        .generation = 7u,
        .track_frame_end = UINT64_C(123456),
        .peak_left = 32768u,
        .peak_right = 1234u,
        .paused = 1u,
        .volume_step = 5u,
    };
    pcm_block_info_t block_out = {0};
    assert(!pcm_queue_read_block(&q, out, &block_out));
    for (uint32_t block = 0; block < PCM_QUEUE_BLOCK_CAPACITY; ++block) {
        pcm_block_info_t tagged = block_in;
        tagged.generation += block;
        assert(pcm_queue_write_block(&q, in, &tagged));
    }
    assert(!pcm_queue_write_block(&q, in, &block_in));
    assert(pcm_queue_available(&q) == PCM_QUEUE_CAPACITY);
    assert(pcm_queue_space(&q) == 0u);
    for (uint32_t block = 0; block < PCM_QUEUE_BLOCK_CAPACITY; ++block) {
        assert(pcm_queue_read_block(&q, out, &block_out));
        assert(memcmp(out, in, sizeof(in)) == 0);
        assert(block_out.generation == block_in.generation + block);
        assert(block_out.track_frame_end == block_in.track_frame_end);
        assert(block_out.paused == block_in.paused);
        assert(block_out.volume_step == block_in.volume_step);
    }
    assert(!pcm_queue_read_block(&q, out, &block_out));
    assert(pcm_queue_available(&q) == 0u);
    assert(pcm_queue_space(&q) == PCM_QUEUE_CAPACITY);

    /* Unsigned block sequence wrap without waiting for billions of blocks. */
    atomic_store(&q.head, UINT32_MAX);
    atomic_store(&q.tail, UINT32_MAX);
    assert(pcm_queue_write_block(&q, in, &block_in));
    assert(pcm_queue_read_block(&q, out, &block_out));
    assert(memcmp(in, out, sizeof(in)) == 0);
}

static void test_status_queue(void) {
    audio_status_queue_t queue;
    audio_status_queue_init(&queue);
    audio_status_snapshot_t snapshot = {0};
    assert(!audio_status_queue_pop_latest(&queue, &snapshot));
    for (uint32_t i = 0; i < AUDIO_STATUS_QUEUE_CAPACITY; ++i) {
        snapshot.completed_frames = i * PCM_BLOCK_FRAMES;
        snapshot.track_frames = UINT64_C(0x100000000) + i;
        assert(audio_status_queue_push(&queue, &snapshot));
    }
    assert(!audio_status_queue_push(&queue, &snapshot));
    snapshot = (audio_status_snapshot_t){0};
    assert(audio_status_queue_pop_latest(&queue, &snapshot));
    assert(snapshot.completed_frames == (AUDIO_STATUS_QUEUE_CAPACITY - 1u) * PCM_BLOCK_FRAMES);
    assert(snapshot.track_frames == UINT64_C(0x100000000) + AUDIO_STATUS_QUEUE_CAPACITY - 1u);
    assert(!audio_status_queue_pop(&queue, &snapshot));
}

static void test_tone(void) {
    test_tone_t a, b;
    uint32_t whole[256], split[256];
    test_tone_init(&a);
    test_tone_init(&b);
    test_tone_render(&a, whole, 256, true);
    test_tone_render(&b, split, 17, true);
    test_tone_render(&b, split + 17, 239, true);
    assert(memcmp(whole, split, sizeof whole) == 0);
    assert(whole[0] == 0u);
    assert(whole[8] == UINT32_C(0x0b501000));
    assert(whole[16] == UINT32_C(0x10000000));
    assert(whole[32] == UINT32_C(0x00000000));
    assert(whole[48] == UINT32_C(0xf0000000));
    for (size_t i = 0; i < 256; ++i) {
        int16_t left = (int16_t)(whole[i] >> 16);
        int16_t right = (int16_t)whole[i];
        assert(left >= -4096 && left <= 4096);
        assert(right >= -4096 && right <= 4096);
    }
    test_tone_render(&a, whole, 256, false);
    for (size_t i = 0; i < 256; ++i)
        assert(whole[i] == 0u);
}

static void test_visual_rate(void) {
    for (uint8_t hz = 30u; hz <= 60u; hz = (uint8_t)(hz + 30u)) {
        visual_rate_t rate = {0};
        visual_rate_set(&rate, hz);
        uint32_t publications = 0u;
        for (uint32_t frames = 0u; frames < 32000u; frames += PCM_BLOCK_FRAMES) {
            if (visual_rate_advance(&rate, PCM_BLOCK_FRAMES))
                ++publications;
        }
        assert(publications == hz);
        assert(rate.phase == 0u);
    }
    visual_rate_t changed = {.requested_hz = 30u, .phase = 123u};
    visual_rate_set(&changed, 60u);
    assert(changed.requested_hz == 60u && changed.phase == 0u);
    visual_rate_set(&changed, 45u);
    assert(changed.requested_hz == 60u);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    if (strcmp(argv[1], "packing") == 0)
        test_packing();
    else if (strcmp(argv[1], "clocks") == 0)
        test_clocks();
    else if (strcmp(argv[1], "queue") == 0)
        test_queue();
    else if (strcmp(argv[1], "status") == 0)
        test_status_queue();
    else if (strcmp(argv[1], "tone") == 0)
        test_tone();
    else if (strcmp(argv[1], "visual-rate") == 0)
        test_visual_rate();
    else
        return 2;
    puts("audio contract OK");
    return 0;
}
