#include "ui.h"

#include <string.h>

#include "ui_dsp.h"
#include "ui_aram.h"
#include "ui_library.h"
#include "ui_marquee.h"
#include "ui_pages.h"
#include "ui_player.h"
#include "ui_voices.h"

static void copy_marquee_title(ui_t *ui) {
    const char *title = ui_track_title(ui);
    strncpy(ui->marquee_title, title, sizeof(ui->marquee_title) - 1u);
    ui->marquee_title[sizeof(ui->marquee_title) - 1u] = '\0';
}

static void draw_header(const ui_t *ui, ui_canvas_t *canvas) {
    ui_draw_text_marquee(canvas, (ui_rect_t){12, 10, 314, 24}, ui_track_title(ui), 3u,
                         UI_COLOR_TEXT, ui->header_offset);
    ui_draw_text_clipped(canvas, (ui_rect_t){12, 43, 314, 16}, ui_game_title(ui), 2u,
                         UI_COLOR_SECONDARY);
    const char *state =
        !ui->model.player_ready
            ? (ui->model.library.error == NULL ? "SPC LOAD FAILED" : ui->model.library.error)
        : ui->model.player_mode == PLAYER_ERROR ? "AUDIO ERROR"
        : ui->command_rejected                  ? "COMMAND BUSY"
        : ui->model.player_mode == PLAYER_FADING_FOR_LOAD || ui->model.player_mode == PLAYER_LOADING
            ? "LOADING"
        : !ui->model.has_track ? "SELECT A TRACK"
        : ui->model.paused     ? (ui->model.source_embedded ? "PAUSED  EMBEDDED" : "PAUSED  SD")
                               : (ui->model.source_embedded ? "PLAYING  EMBEDDED" : "PLAYING  SD");
    const uint8_t color =
        (!ui->model.player_ready || ui->command_rejected || ui->model.player_mode == PLAYER_ERROR)
            ? UI_COLOR_ERROR
            : UI_COLOR_ACCENT;
    ui_draw_text_clipped(canvas, (ui_rect_t){12, 69, 314, 16}, state, 2u, color);
    ui_draw_button(canvas,
                   ui_target_rect(ui->screen, ui->screen == UI_SCREEN_PLAYER ? UI_TARGET_VIEW
                                                                             : UI_TARGET_CLOSE),
                   ui->screen == UI_SCREEN_PLAYER ? "VIEW" : "CLOSE", false,
                   ui->pressed_target ==
                       (ui->screen == UI_SCREEN_PLAYER ? UI_TARGET_VIEW : UI_TARGET_CLOSE));
}

static void draw_transport(const ui_t *ui, ui_canvas_t *canvas) {
    ui_draw_button(canvas, UI_RECT_VOLUME, "VOLUME", ui->screen == UI_SCREEN_VOLUME,
                   ui->pressed_target == UI_TARGET_VOLUME);
    ui_draw_button(canvas, UI_RECT_PLAY_PAUSE,
                   !ui->model.has_track || ui->model.paused ? "PLAY" : "PAUSE", false,
                   ui->pressed_target == UI_TARGET_PLAY_PAUSE);
    ui_draw_button(canvas, UI_RECT_RESTART, "RESTART", false,
                   ui->pressed_target == UI_TARGET_RESTART);
}

void ui_init(ui_t *ui, const ui_model_t *model) {
    *ui = (ui_t){0};
    ui->screen = UI_SCREEN_PLAYER;
    ui->model = *model;
    ui->header_epoch_ms = model->animation_ms;
    ui->track_epoch_ms = model->animation_ms;
    copy_marquee_title(ui);
    ui->library.prepared_folder_revision = UINT32_MAX;
    ui_library_sync(&ui->library, &ui->model.library);
    ui_touch_init(&ui->touch);
}

bool ui_set_model(ui_t *ui, const ui_model_t *model) {
    const bool track_changed = ui->model.generation != model->generation;
    const bool command_completed =
        ui->model.player_ready != model->player_ready || ui->model.has_track != model->has_track ||
        ui->model.player_mode != model->player_mode || ui->model.paused != model->paused ||
        ui->model.volume_step != model->volume_step || ui->model.generation != model->generation;
    const bool changed =
        command_completed || ui->model.audio_ready != model->audio_ready ||
        ui->model.silence_frames != model->silence_frames ||
        ui->model.underrun_blocks != model->underrun_blocks ||
        ui->model.late_dma_blocks != model->late_dma_blocks ||
        ui->model.dropped_snapshots != model->dropped_snapshots ||
        ui->model.dropped_visual_snapshots != model->dropped_visual_snapshots ||
        ui->model.min_buffered_frames != model->min_buffered_frames ||
        ui->model.max_render_us != model->max_render_us ||
        ui->model.max_gui_slice_us != model->max_gui_slice_us ||
        ui->model.display_last_transfer_us != model->display_last_transfer_us ||
        ui->model.display_max_transfer_us != model->display_max_transfer_us ||
        ui->model.measured_aram_hz_x10 != model->measured_aram_hz_x10 ||
        ui->model.visualizer_hz != model->visualizer_hz ||
        ui->model.aram_sequence != model->aram_sequence ||
        ui->model.snapshot_sequence != model->snapshot_sequence ||
        ui->model.track_frames != model->track_frames || ui->model.peak_left != model->peak_left ||
        ui->model.peak_right != model->peak_right || ui->model.metadata != model->metadata ||
        ui->model.library.state != model->library.state ||
        ui->model.library.has_current_track != model->library.has_current_track;
    ui->model = *model;
    if (track_changed || strcmp(ui->marquee_title, ui_track_title(ui)) != 0) {
        copy_marquee_title(ui);
        ui->header_epoch_ms = model->animation_ms;
        ui->track_epoch_ms = model->animation_ms;
        ui->header_offset = 0;
        ui->track_offset = 0;
    }
    ui_library_sync(&ui->library, &ui->model.library);
    if (command_completed)
        ui->command_rejected = false;
    return changed;
}

void ui_render(const ui_t *ui, ui_canvas_t *canvas) {
    ui_canvas_clear(canvas, UI_COLOR_BACKGROUND);
    draw_header(ui, canvas);
    switch (ui->screen) {
    case UI_SCREEN_PLAYER:
        ui_player_draw(ui, canvas);
        break;
    case UI_SCREEN_VIEW_MENU:
        ui_view_menu_draw(ui, canvas);
        break;
    case UI_SCREEN_ARAM:
        ui_aram_draw(ui, canvas);
        break;
    case UI_SCREEN_VOICES: {
        const uint32_t voice_validity =
            SPC_SNAPSHOT_VALID_DSP_REGISTERS | SPC_SNAPSHOT_VALID_VOICE_INTERNALS;
        const bool valid = ui->model.spc_snapshot != NULL &&
                           ui->model.spc_snapshot->generation == ui->model.generation &&
                           (ui->model.spc_snapshot->validity & voice_validity) == voice_validity;
        ui_voices_draw_bank(canvas, ui->model.spc_snapshot, valid, ui->voice_bank,
                            ui->pressed_target);
        break;
    }
    case UI_SCREEN_VOICE_DETAIL: {
        const uint32_t voice_validity =
            SPC_SNAPSHOT_VALID_DSP_REGISTERS | SPC_SNAPSHOT_VALID_VOICE_INTERNALS;
        const bool valid = ui->model.spc_snapshot != NULL &&
                           ui->model.spc_snapshot->generation == ui->model.generation &&
                           (ui->model.spc_snapshot->validity & voice_validity) == voice_validity;
        ui_voices_draw_detail(canvas, ui->model.spc_snapshot, valid, ui->voice_number);
        break;
    }
    case UI_SCREEN_DSP: {
        const bool valid =
            ui->model.spc_snapshot != NULL &&
            ui->model.spc_snapshot->generation == ui->model.generation &&
            (ui->model.spc_snapshot->validity & SPC_SNAPSHOT_VALID_DSP_REGISTERS) != 0u;
        ui_dsp_draw(canvas, ui->model.spc_snapshot, valid, ui->dsp_page, ui->pressed_target);
        break;
    }
    case UI_SCREEN_TRACK:
        ui_track_draw(ui, canvas);
        break;
    case UI_SCREEN_LIBRARY:
        ui_library_draw(canvas, &ui->model.library, &ui->library, ui->pressed_target);
        break;
    case UI_SCREEN_SETTINGS:
        ui_settings_draw(ui, canvas);
        break;
    case UI_SCREEN_VOLUME:
        ui_volume_draw(ui, canvas);
        break;
    default:
        break;
    }
    draw_transport(ui, canvas);
}

void ui_render_dirty(const ui_t *ui, ui_canvas_t *canvas, ui_dirty_t dirty) {
    if (!dirty.changed)
        return;
    if (dirty.full) {
        ui_canvas_reset_clip(canvas);
        ui_render(ui, canvas);
        return;
    }
    ui_canvas_set_clip(canvas, dirty.rect);
    ui_render(ui, canvas);
    ui_canvas_reset_clip(canvas);
}

static bool enqueue(ui_t *ui, player_command_queue_t *commands, player_command_t command) {
    const bool accepted = player_command_push(commands, command);
    ui->command_rejected = !accepted;
    return accepted;
}

static void request_aram_mode(ui_t *ui, bool data_mode) {
    ++ui->aram_mode_serial;
    if (ui->aram_mode_serial == 0u)
        ++ui->aram_mode_serial;
    ui->aram_data_mode = data_mode;
    ui->aram_mode_request = (ui->aram_mode_serial << 1u) | (data_mode ? 1u : 0u);
    ui->aram_mode_request_pending = true;
}

static void leave_player_aram_data(ui_t *ui) {
    if (ui->screen == UI_SCREEN_PLAYER && ui->aram_data_mode)
        request_aram_mode(ui, false);
}

static bool activate(ui_t *ui, ui_target_t target, player_command_queue_t *commands) {
    switch (target) {
    case UI_TARGET_VIEW:
        leave_player_aram_data(ui);
        ui->screen = UI_SCREEN_VIEW_MENU;
        return true;
    case UI_TARGET_CLOSE:
        if (ui->screen == UI_SCREEN_ARAM)
            request_aram_mode(ui, false);
        ui->screen = ui->screen == UI_SCREEN_VOICE_DETAIL ? UI_SCREEN_VOICES : UI_SCREEN_PLAYER;
        return true;
    case UI_TARGET_VOLUME:
        if (ui->screen == UI_SCREEN_ARAM)
            request_aram_mode(ui, false);
        leave_player_aram_data(ui);
        ui->screen = UI_SCREEN_VOLUME;
        return true;
    case UI_TARGET_ARAM:
        request_aram_mode(ui, false);
        ui->screen = UI_SCREEN_ARAM;
        return true;
    case UI_TARGET_ARAM_MAP:
        request_aram_mode(ui, !ui->aram_data_mode);
        return true;
    case UI_TARGET_VOICES:
        ui->screen = UI_SCREEN_VOICES;
        return true;
    case UI_TARGET_DSP:
        ui->screen = UI_SCREEN_DSP;
        return true;
    case UI_TARGET_TRACK:
        ui->screen = UI_SCREEN_TRACK;
        return true;
    case UI_TARGET_LIBRARY:
        ui->screen = UI_SCREEN_LIBRARY;
        return true;
    case UI_TARGET_SETTINGS:
        ui->screen = UI_SCREEN_SETTINGS;
        return true;
    case UI_TARGET_VISUALIZER_RATE:
        ui->model.visualizer_hz = ui->model.visualizer_hz == 60u ? 30u : 60u;
        ui->requested_visual_rate = ui->model.visualizer_hz;
        ui->visual_rate_request_pending = true;
        return false;
    case UI_TARGET_VOICE_CARD_1:
    case UI_TARGET_VOICE_CARD_2:
    case UI_TARGET_VOICE_CARD_3:
    case UI_TARGET_VOICE_CARD_4:
        ui->voice_number =
            (uint8_t)(ui->voice_bank * 4u + (uint8_t)(target - UI_TARGET_VOICE_CARD_1));
        ui->screen = UI_SCREEN_VOICE_DETAIL;
        return true;
    case UI_TARGET_VOICE_BANK_1_4:
        ui->voice_bank = 0u;
        return true;
    case UI_TARGET_VOICE_BANK_5_8:
        ui->voice_bank = 1u;
        return true;
    case UI_TARGET_DSP_MIX:
        ui->dsp_page = 0u;
        return true;
    case UI_TARGET_DSP_REGISTERS:
        ui->dsp_page = 1u;
        return true;
    case UI_TARGET_LIBRARY_ROW_1:
    case UI_TARGET_LIBRARY_ROW_2:
    case UI_TARGET_LIBRARY_ROW_3:
    case UI_TARGET_LIBRARY_BACK:
    case UI_TARGET_LIBRARY_PREV:
    case UI_TARGET_LIBRARY_NEXT: {
        const ui_library_action_t action =
            ui_library_activate(&ui->library, &ui->model.library, target);
        if (action.track_selected) {
            ui->requested_track = action.track;
            ui->track_request_pending = true;
        }
        if (action.folder_selected) {
            ui->requested_folder = action.folder_index;
            ui->folder_request_pending = true;
        }
        if (action.embedded_selected) {
            (void)enqueue(ui, commands, (player_command_t){PLAYER_COMMAND_LOAD_EMBEDDED, 0u});
        }
        return action.changed;
    }
    case UI_TARGET_PLAY_PAUSE:
        if (!ui->model.has_track) {
            if (ui->model.library.show_embedded) {
                (void)enqueue(ui, commands, (player_command_t){PLAYER_COMMAND_LOAD_EMBEDDED, 0u});
            } else {
                leave_player_aram_data(ui);
                ui->screen = UI_SCREEN_LIBRARY;
                return true;
            }
        } else {
            (void)enqueue(
                ui, commands,
                (player_command_t){PLAYER_COMMAND_SET_PAUSED, (uint8_t)!ui->model.paused});
        }
        return false;
    case UI_TARGET_RESTART:
        if (ui->model.source_embedded) {
            (void)enqueue(ui, commands, (player_command_t){PLAYER_COMMAND_RESTART, 0u});
        } else if (ui->model.library.has_current_track) {
            ui->restart_request_pending = true;
        }
        return false;
    case UI_TARGET_VOLUME_DOWN:
        if (ui->model.volume_step != 0u) {
            (void)enqueue(ui, commands,
                          (player_command_t){PLAYER_COMMAND_SET_VOLUME,
                                             (uint8_t)(ui->model.volume_step - 1u)});
        }
        return false;
    case UI_TARGET_VOLUME_UP:
        if (ui->model.volume_step < 8u) {
            (void)enqueue(ui, commands,
                          (player_command_t){PLAYER_COMMAND_SET_VOLUME,
                                             (uint8_t)(ui->model.volume_step + 1u)});
        }
        return false;
    default:
        return false;
    }
}

bool ui_take_folder_request(ui_t *ui, uint16_t *folder_index) {
    if (ui == NULL || folder_index == NULL || !ui->folder_request_pending) {
        return false;
    }
    *folder_index = ui->requested_folder;
    ui->folder_request_pending = false;
    return true;
}

bool ui_take_track_request(ui_t *ui, spc_track_ref_t *track) {
    if (ui == NULL || track == NULL || !ui->track_request_pending) {
        return false;
    }
    *track = ui->requested_track;
    ui->track_request_pending = false;
    return true;
}

bool ui_take_restart_request(ui_t *ui) {
    if (ui == NULL || !ui->restart_request_pending)
        return false;
    ui->restart_request_pending = false;
    return true;
}

bool ui_take_visible_title_request(ui_t *ui, uint32_t *page_revision, spc_track_ref_t *tracks,
                                   uint8_t *count) {
    if (ui == NULL || ui->screen != UI_SCREEN_LIBRARY)
        return false;
    return ui_library_take_title_request(&ui->library, page_revision, tracks, count);
}

ui_dirty_t ui_accept_visible_title(ui_t *ui, const visible_title_t *title) {
    if (ui == NULL || !ui_library_accept_title(&ui->library, title, ui->model.animation_ms)) {
        return (ui_dirty_t){0};
    }
    if (ui->screen != UI_SCREEN_LIBRARY)
        return (ui_dirty_t){0};
    return (ui_dirty_t){
        true, false,
        ui_target_rect(UI_SCREEN_LIBRARY, (ui_target_t)(UI_TARGET_LIBRARY_ROW_1 + title->row))};
}

static void include_dirty(ui_dirty_t *dirty, ui_rect_t rect) {
    if (!dirty->changed) {
        *dirty = (ui_dirty_t){true, false, rect};
        return;
    }
    const int16_t x0 = dirty->rect.x < rect.x ? dirty->rect.x : rect.x;
    const int16_t y0 = dirty->rect.y < rect.y ? dirty->rect.y : rect.y;
    const int32_t old_x1 = (int32_t)dirty->rect.x + dirty->rect.width;
    const int32_t old_y1 = (int32_t)dirty->rect.y + dirty->rect.height;
    const int32_t new_x1 = (int32_t)rect.x + rect.width;
    const int32_t new_y1 = (int32_t)rect.y + rect.height;
    dirty->rect = (ui_rect_t){x0, y0, (int16_t)((old_x1 > new_x1 ? old_x1 : new_x1) - x0),
                              (int16_t)((old_y1 > new_y1 ? old_y1 : new_y1) - y0)};
}

ui_dirty_t ui_update_animation(ui_t *ui, uint32_t animation_ms) {
    ui_dirty_t dirty = {0};
    if (ui == NULL)
        return dirty;
    ui->model.animation_ms = animation_ms;
    ui->model.library.animation_ms = animation_ms;
    const int16_t header = ui_marquee_offset(ui_text_width(ui_track_title(ui), 3u), 314u,
                                             animation_ms - ui->header_epoch_ms);
    if (header != ui->header_offset) {
        ui->header_offset = header;
        include_dirty(&dirty, (ui_rect_t){12, 10, 314, 24});
    }
    if (ui->screen == UI_SCREEN_TRACK) {
        const int16_t track = ui_marquee_offset(ui_text_width(ui_track_title(ui), 3u), 426u,
                                                animation_ms - ui->track_epoch_ms);
        if (track != ui->track_offset) {
            ui->track_offset = track;
            include_dirty(&dirty, (ui_rect_t){12, 134, 426, 24});
        }
    }
    if (ui->screen == UI_SCREEN_LIBRARY &&
        ui_library_update_offsets(&ui->library, &ui->model.library)) {
        include_dirty(&dirty, (ui_rect_t){12, 146, 426, 282});
    }
    return dirty;
}

bool ui_take_visual_rate_request(ui_t *ui, uint8_t *requested_hz) {
    if (ui == NULL || requested_hz == NULL || !ui->visual_rate_request_pending)
        return false;
    *requested_hz = ui->requested_visual_rate;
    ui->visual_rate_request_pending = false;
    return true;
}

bool ui_take_aram_mode_request(ui_t *ui, uint32_t *request) {
    if (ui == NULL || request == NULL || !ui->aram_mode_request_pending)
        return false;
    *request = ui->aram_mode_request;
    ui->aram_mode_request_pending = false;
    return true;
}

ui_dirty_t ui_handle_touch(ui_t *ui, uint16_t x, uint16_t y, bool pressed,
                           player_command_queue_t *commands) {
    const ui_touch_event_t event = ui_touch_update(&ui->touch, ui->screen, x, y, pressed);
    if (event.type == UI_TOUCH_NO_EVENT)
        return (ui_dirty_t){0};

    ui_rect_t old_rect = ui_target_rect(ui->screen, event.target);
    if (event.target == UI_TARGET_ARAM_MAP) {
        /* Routine map updates exclude the border; touch feedback owns it. */
        old_rect.x = (int16_t)(old_rect.x - 2);
        old_rect.y = (int16_t)(old_rect.y - 2);
        old_rect.width = (int16_t)(old_rect.width + 4);
        old_rect.height = (int16_t)(old_rect.height + 4);
    }
    if (event.type == UI_TOUCH_PRESS_CHANGED) {
        ui->pressed_target = pressed ? event.target : UI_TARGET_NONE;
        return (ui_dirty_t){true, false, old_rect};
    }
    if (event.type == UI_TOUCH_CANCELLED) {
        ui->pressed_target = UI_TARGET_NONE;
        return (ui_dirty_t){true, false, old_rect};
    }

    ui->pressed_target = UI_TARGET_NONE;
    const bool screen_changed = activate(ui, event.target, commands);
    if (event.target == UI_TARGET_ARAM_MAP) {
        const int16_t height = ui->screen == UI_SCREEN_PLAYER ? 310 : 380;
        return (ui_dirty_t){true, false, {0, 100, 450, height}};
    }
    return (ui_dirty_t){true, screen_changed,
                        screen_changed ? (ui_rect_t){0, 0, 450, 600} : old_rect};
}
