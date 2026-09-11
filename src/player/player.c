#include "player.h"

#include <string.h>

#define PLAYER_VOLUME_STEPS 8u
#define PLAYER_GAIN_ONE_Q15 32768
#define PLAYER_RAMP_FRAMES 256u

static bool paused_mode(player_mode_t mode) {
    return mode == PLAYER_PAUSED || mode == PLAYER_FADING_FOR_PAUSE;
}

static void publish_status(const player_t *player) {
    atomic_store_explicit(&player->shared->ready, player->ready, memory_order_release);
    atomic_store_explicit(&player->shared->paused, paused_mode(player->mode), memory_order_release);
    atomic_store_explicit(&player->shared->has_track, player->has_track, memory_order_release);
    atomic_store_explicit(&player->shared->mode, (uint32_t)player->mode, memory_order_release);
    atomic_store_explicit(&player->shared->volume_step, player->volume_step, memory_order_release);
    atomic_store_explicit(&player->shared->generation, player->generation, memory_order_release);
}

static void set_volume(player_t *player, uint8_t step) {
    if (step > PLAYER_VOLUME_STEPS)
        step = PLAYER_VOLUME_STEPS;
    player->volume_step = step;
    player->volume_gain_q15 = (int32_t)step * PLAYER_GAIN_ONE_Q15 / (int32_t)PLAYER_VOLUME_STEPS;
    if (player->mode == PLAYER_PLAYING || player->mode == PLAYER_FADING_IN) {
        player->target_gain_q15 = player->volume_gain_q15;
        player->ramp_frames = PLAYER_RAMP_FRAMES;
    }
}

static void begin_load(player_t *player, const uint8_t *image, size_t size, bool from_mailbox) {
    player->pending_image = image;
    player->pending_image_size = size;
    player->pending_from_mailbox = from_mailbox;
    player->restore_mode = paused_mode(player->mode) ? PLAYER_PAUSED
                           : player->has_track       ? PLAYER_PLAYING
                                                     : PLAYER_IDLE;
    player->restore_is_embedded = player->current_is_embedded;
    player->desired_paused_after_load = player->restore_mode == PLAYER_PAUSED;
    player->mode = PLAYER_FADING_FOR_LOAD;
    player->target_gain_q15 = 0;
    player->ramp_frames = player->gain_q15 == 0 ? 0u : PLAYER_RAMP_FRAMES;
}

static void claim_storage_image(player_t *player) {
    if (player->storage_mailbox == NULL || player->mode == PLAYER_FADING_FOR_LOAD ||
        player->mode == PLAYER_LOADING)
        return;
    const uint8_t *image = NULL;
    size_t size = 0u;
    spc_track_ref_t track = {0};
    if (storage_mailbox_begin_read(player->storage_mailbox, &image, &size, &track)) {
        (void)track;
        begin_load(player, image, size, true);
    }
}

static void process_commands(player_t *player) {
    player_command_t command;
    /* Bound work even if Core 1 enqueues more commands while we drain. */
    for (uint32_t handled = 0u;
         handled < PLAYER_COMMAND_CAPACITY && player_command_pop(player->commands, &command);
         ++handled) {
        switch (command.type) {
        case PLAYER_COMMAND_SET_PAUSED:
            if (player->mode == PLAYER_FADING_FOR_LOAD || player->mode == PLAYER_LOADING) {
                player->desired_paused_after_load = command.value != 0u;
            } else if (player->mode == PLAYER_FADING_FOR_RESTART) {
                player->restore_mode = command.value != 0u ? PLAYER_PAUSED : PLAYER_PLAYING;
            } else if (!player->has_track) {
                break;
            } else if (command.value != 0u && player->mode != PLAYER_PAUSED &&
                       player->mode != PLAYER_FADING_FOR_PAUSE) {
                player->mode = PLAYER_FADING_FOR_PAUSE;
                player->target_gain_q15 = 0;
                player->ramp_frames = player->gain_q15 == 0 ? 0u : PLAYER_RAMP_FRAMES;
            } else if (command.value == 0u &&
                       (player->mode == PLAYER_PAUSED || player->mode == PLAYER_FADING_FOR_PAUSE)) {
                player->mode = PLAYER_FADING_IN;
                /* Reverse a pending fade from its current gain. */
                player->target_gain_q15 = player->volume_gain_q15;
                player->ramp_frames = PLAYER_RAMP_FRAMES;
            }
            break;
        case PLAYER_COMMAND_RESTART:
            if (player->has_track && player->current_is_embedded &&
                player->mode != PLAYER_FADING_FOR_LOAD && player->mode != PLAYER_LOADING &&
                player->mode != PLAYER_FADING_FOR_RESTART) {
                player->restore_mode = paused_mode(player->mode) ? PLAYER_PAUSED : PLAYER_PLAYING;
                player->mode = PLAYER_FADING_FOR_RESTART;
                player->target_gain_q15 = 0;
                player->ramp_frames = player->gain_q15 == 0 ? 0u : PLAYER_RAMP_FRAMES;
            }
            break;
        case PLAYER_COMMAND_SET_VOLUME:
            set_volume(player, command.value);
            break;
        case PLAYER_COMMAND_LOAD_EMBEDDED:
            if (player->embedded_image != NULL && player->mode != PLAYER_FADING_FOR_LOAD &&
                player->mode != PLAYER_LOADING) {
                begin_load(player, player->embedded_image, player->embedded_image_size, false);
            }
            break;
        default:
            break;
        }
    }
    publish_status(player);
}

static void finish_load(player_t *player) {
    player->mode = PLAYER_LOADING;
    const bool accepted =
        player->backend != NULL && software_spc_load(player->backend, player->pending_image,
                                                     player->pending_image_size, 1) == NULL;
    player->error = accepted ? NULL : "SPC LOAD FAILED";
    const bool from_mailbox = player->pending_from_mailbox;
    player->pending_image = NULL;
    player->pending_image_size = 0u;
    player->pending_from_mailbox = false;
    if (accepted) {
        player->has_track = true;
        player->current_is_embedded = !from_mailbox;
        ++player->generation;
        if (player->desired_paused_after_load) {
            player->mode = PLAYER_PAUSED;
            player->target_gain_q15 = 0;
            player->ramp_frames = 0u;
        } else {
            player->mode = PLAYER_FADING_IN;
            player->target_gain_q15 = player->volume_gain_q15;
            player->ramp_frames = PLAYER_RAMP_FRAMES;
        }
    } else {
        player->current_is_embedded = player->restore_is_embedded;
        player->mode = !player->has_track                  ? PLAYER_IDLE
                       : player->desired_paused_after_load ? PLAYER_PAUSED
                                                           : PLAYER_FADING_IN;
        player->target_gain_q15 = player->mode == PLAYER_PAUSED ? 0 : player->volume_gain_q15;
        player->ramp_frames = player->mode == PLAYER_IDLE ? 0u : PLAYER_RAMP_FRAMES;
    }
    if (from_mailbox) {
        storage_mailbox_complete_read(player->storage_mailbox, accepted, player->generation);
    }
    publish_status(player);
}

static void complete_silent_transition(player_t *player) {
    if (player->gain_q15 != 0)
        return;
    if (player->mode == PLAYER_FADING_FOR_LOAD) {
        finish_load(player);
    } else if (player->mode == PLAYER_FADING_FOR_RESTART) {
        const char *error = software_spc_load(player->backend, player->embedded_image,
                                              player->embedded_image_size, 1);
        if (error == NULL) {
            ++player->generation;
            if (player->restore_mode == PLAYER_PAUSED) {
                player->mode = PLAYER_PAUSED;
                player->ramp_frames = 0u;
            } else {
                player->mode = PLAYER_FADING_IN;
                player->target_gain_q15 = player->volume_gain_q15;
                player->ramp_frames = PLAYER_RAMP_FRAMES;
            }
        } else {
            player->error = error;
            player->mode = PLAYER_ERROR;
        }
        publish_status(player);
    } else if (player->mode == PLAYER_FADING_FOR_PAUSE) {
        player->mode = PLAYER_PAUSED;
        player->ramp_frames = 0u;
        publish_status(player);
    }
}

const char *player_init_idle(player_t *player, const uint8_t *embedded_image, size_t embedded_size,
                             player_command_queue_t *commands, player_shared_status_t *shared) {
    if (player == NULL || commands == NULL || shared == NULL) {
        return "invalid player arguments";
    }
    memset(player, 0, sizeof(*player));
    player->embedded_image = embedded_image;
    player->embedded_image_size = embedded_size;
    player->commands = commands;
    player->shared = shared;
    player->volume_step = PLAYER_VOLUME_STEPS;
    player->volume_gain_q15 = PLAYER_GAIN_ONE_Q15;
    player->mode = PLAYER_IDLE;
    player->backend = software_spc_create();
    player->ready = player->backend != NULL;
    player->error = player->ready ? NULL : "emulator allocation failed";
    atomic_init(&shared->ready, player->ready);
    atomic_init(&shared->paused, false);
    atomic_init(&shared->has_track, false);
    atomic_init(&shared->mode, (uint32_t)player->mode);
    atomic_init(&shared->volume_step, PLAYER_VOLUME_STEPS);
    atomic_init(&shared->generation, 0u);
    return player->error;
}

void player_bind_storage_mailbox(player_t *player, storage_mailbox_t *mailbox) {
    if (player != NULL)
        player->storage_mailbox = mailbox;
}

bool player_render_block(player_t *player, uint32_t *packed_frames, pcm_block_info_t *block_info) {
    if (player == NULL || packed_frames == NULL || block_info == NULL) {
        return false;
    }
    claim_storage_image(player);
    process_commands(player);
    complete_silent_transition(player);
    const bool silent = !player->ready || !player->has_track || player->mode == PLAYER_IDLE ||
                        player->mode == PLAYER_PAUSED || player->mode == PLAYER_LOADING ||
                        player->mode == PLAYER_ERROR;
    *block_info = (pcm_block_info_t){
        .generation = player->generation,
        .track_frame_end = player->has_track ? software_spc_generated_frames(player->backend) : 0u,
        .paused = paused_mode(player->mode),
        .mode = (uint8_t)player->mode,
        .volume_step = player->volume_step,
        .intentional_silence = silent,
    };
    if (silent) {
        memset(packed_frames, 0, PCM_BLOCK_FRAMES * sizeof(*packed_frames));
        return player->ready;
    }

    player->error = software_spc_render(player->backend, player->samples, PCM_BLOCK_FRAMES);
    if (player->error != NULL) {
        player->mode = PLAYER_ERROR;
        block_info->mode = (uint8_t)PLAYER_ERROR;
        publish_status(player);
        memset(packed_frames, 0, PCM_BLOCK_FRAMES * sizeof(*packed_frames));
        block_info->intentional_silence = true;
        return false;
    }
    for (size_t frame = 0; frame < PCM_BLOCK_FRAMES; ++frame) {
        if (player->ramp_frames != 0u) {
            player->gain_q15 +=
                (player->target_gain_q15 - player->gain_q15) / (int32_t)player->ramp_frames;
            --player->ramp_frames;
            if (player->ramp_frames == 0u) {
                player->gain_q15 = player->target_gain_q15;
                if (player->mode == PLAYER_FADING_IN)
                    player->mode = PLAYER_PLAYING;
            }
        }
        const int32_t left =
            (int32_t)player->samples[frame * 2u] * player->gain_q15 / PLAYER_GAIN_ONE_Q15;
        const int32_t right =
            (int32_t)player->samples[frame * 2u + 1u] * player->gain_q15 / PLAYER_GAIN_ONE_Q15;
        const int16_t scaled_left = (int16_t)left;
        const int16_t scaled_right = (int16_t)right;
        const uint16_t left_peak = pcm_peak_magnitude(scaled_left);
        const uint16_t right_peak = pcm_peak_magnitude(scaled_right);
        if (left_peak > block_info->peak_left)
            block_info->peak_left = left_peak;
        if (right_peak > block_info->peak_right)
            block_info->peak_right = right_peak;
        packed_frames[frame] = pcm_pack_i2s(scaled_left, scaled_right);
    }
    block_info->track_frame_end = software_spc_generated_frames(player->backend);
    block_info->mode = (uint8_t)player->mode;
    complete_silent_transition(player);
    publish_status(player);
    return true;
}

player_status_t player_status_read(const player_shared_status_t *shared) {
    return (player_status_t){
        .ready = atomic_load_explicit(&shared->ready, memory_order_acquire),
        .paused = atomic_load_explicit(&shared->paused, memory_order_acquire),
        .has_track = atomic_load_explicit(&shared->has_track, memory_order_acquire),
        .mode = (player_mode_t)atomic_load_explicit(&shared->mode, memory_order_acquire),
        .volume_step = (uint8_t)atomic_load_explicit(&shared->volume_step, memory_order_acquire),
        .generation = atomic_load_explicit(&shared->generation, memory_order_acquire),
    };
}
