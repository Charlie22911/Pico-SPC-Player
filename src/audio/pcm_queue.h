#ifndef PCM_QUEUE_H
#define PCM_QUEUE_H
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "pcm_format.h"

#define PCM_QUEUE_CAPACITY 2048u
#define PCM_QUEUE_BLOCK_CAPACITY (PCM_QUEUE_CAPACITY / PCM_BLOCK_FRAMES)

typedef struct {
    uint32_t generation;
    uint64_t track_frame_end;
    uint16_t peak_left;
    uint16_t peak_right;
    uint8_t paused;
    uint8_t mode;
    uint8_t volume_step;
    bool intentional_silence;
} pcm_block_info_t;

/* Core 0 produces complete PCM blocks and the I2S DMA IRQ consumes them.
 * Counts are stereo frames. Initialization is valid only while both endpoints
 * are stopped; sequence arithmetic remains valid across uint32_t rollover. */
typedef struct {
    _Atomic uint32_t head;
    _Atomic uint32_t tail;
    struct {
        uint32_t frames[PCM_BLOCK_FRAMES];
        pcm_block_info_t info;
    } blocks[PCM_QUEUE_BLOCK_CAPACITY];
} pcm_queue_t;
void pcm_queue_init(pcm_queue_t *queue);
size_t pcm_queue_available(const pcm_queue_t *queue);
size_t pcm_queue_space(const pcm_queue_t *queue);
bool pcm_queue_write_block(pcm_queue_t *queue, const uint32_t *frames,
                           const pcm_block_info_t *info);
bool pcm_queue_read_block(pcm_queue_t *queue, uint32_t *frames, pcm_block_info_t *info);
#endif
