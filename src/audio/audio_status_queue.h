#ifndef AUDIO_STATUS_QUEUE_H
#define AUDIO_STATUS_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#define AUDIO_STATUS_QUEUE_CAPACITY 8u

typedef struct {
    uint32_t completed_frames;
    uint32_t silence_frames;
    uint32_t underrun_blocks;
    uint32_t late_dma_blocks;
    uint32_t dropped_snapshots;
    uint32_t min_buffered_frames;
    uint32_t max_render_us;
    uint32_t generation;
    uint64_t track_frames;
    uint16_t peak_left;
    uint16_t peak_right;
    bool paused;
    bool has_track;
    uint8_t mode;
    uint8_t volume_step;
} audio_status_snapshot_t;

/* The audio IRQ produces copied diagnostics and Core 1 consumes them. Neither
 * endpoint waits; a full queue drops a diagnostic snapshot, and pop_latest()
 * may discard older entries in favor of the newest available state. */
typedef struct {
    _Atomic uint32_t head;
    _Atomic uint32_t tail;
    audio_status_snapshot_t entries[AUDIO_STATUS_QUEUE_CAPACITY];
} audio_status_queue_t;

void audio_status_queue_init(audio_status_queue_t *queue);
bool audio_status_queue_push(audio_status_queue_t *queue, const audio_status_snapshot_t *snapshot);
bool audio_status_queue_pop(audio_status_queue_t *queue, audio_status_snapshot_t *snapshot);
bool audio_status_queue_pop_latest(audio_status_queue_t *queue, audio_status_snapshot_t *snapshot);

#endif
