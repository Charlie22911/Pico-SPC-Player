#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "pcm_queue.h"
#include "player_commands.h"
#include "software_spc_backend.h"
#include "storage/storage_mailbox.h"

typedef struct {
    _Atomic bool ready;
    _Atomic bool paused;
    _Atomic bool has_track;
    _Atomic uint32_t mode;
    _Atomic uint32_t volume_step;
    _Atomic uint32_t generation;
} player_shared_status_t;

typedef struct {
    bool ready;
    bool paused;
    bool has_track;
    player_mode_t mode;
    uint8_t volume_step;
    uint32_t generation;
} player_status_t;

typedef struct {
    software_spc_backend_t *backend;
    const uint8_t *embedded_image;
    size_t embedded_image_size;
    player_command_queue_t *commands;
    player_shared_status_t *shared;
    storage_mailbox_t *storage_mailbox;
    const uint8_t *pending_image;
    size_t pending_image_size;
    const char *error;
    player_mode_t mode;
    player_mode_t restore_mode;
    bool ready;
    bool has_track;
    bool current_is_embedded;
    bool restore_is_embedded;
    bool pending_from_mailbox;
    bool desired_paused_after_load;
    uint8_t volume_step;
    uint32_t generation;
    int32_t gain_q15;
    int32_t target_gain_q15;
    int32_t volume_gain_q15;
    uint16_t ramp_frames;
    int16_t samples[PCM_BLOCK_FRAMES * SOFTWARE_SPC_CHANNELS];
} player_t;

const char *player_init_idle(player_t *player, const uint8_t *embedded_image, size_t embedded_size,
                             player_command_queue_t *commands, player_shared_status_t *shared);
void player_bind_storage_mailbox(player_t *player, storage_mailbox_t *mailbox);
bool player_render_block(player_t *player, uint32_t *packed_frames, pcm_block_info_t *block_info);
player_status_t player_status_read(const player_shared_status_t *shared);

#endif
