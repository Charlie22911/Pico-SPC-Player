#include "app/ui_runtime.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "AMOLED_2in41.h"
#include "audio/i2s_output.h"
#include "board/board.h"
#include "display/display.h"
#include "ui/ui.h"
#include "ui/ui_aram.h"
#include "visualizer/aram_view.h"

#define UI_WIDTH AMOLED_2IN41_WIDTH
#define UI_HEIGHT AMOLED_2IN41_HEIGHT
#define UI_DIRTY_CAPACITY 8u
#define UI_TIMEBASE_US 1000000u
#define UI_TOUCH_POLL_MS 10u
#define UI_DISPLAY_RETRY_MS 1000u

typedef struct {
    ui_dirty_t entries[UI_DIRTY_CAPACITY];
    uint8_t count;
} ui_dirty_queue_t;

static uint8_t ui_framebuffer[UI_INDEXED4_STRIDE(UI_WIDTH) * UI_HEIGHT];

static ui_rect_t rect_union(ui_rect_t a, ui_rect_t b) {
    const int32_t x0 = a.x < b.x ? a.x : b.x;
    const int32_t y0 = a.y < b.y ? a.y : b.y;
    const int32_t ax1 = (int32_t)a.x + a.width;
    const int32_t ay1 = (int32_t)a.y + a.height;
    const int32_t bx1 = (int32_t)b.x + b.width;
    const int32_t by1 = (int32_t)b.y + b.height;
    const int32_t x1 = ax1 > bx1 ? ax1 : bx1;
    const int32_t y1 = ay1 > by1 ? ay1 : by1;
    return (ui_rect_t){(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
}

static bool rects_touch(ui_rect_t a, ui_rect_t b) {
    return a.x <= (int32_t)b.x + b.width && b.x <= (int32_t)a.x + a.width &&
           a.y <= (int32_t)b.y + b.height && b.y <= (int32_t)a.y + a.height;
}

static void queue_dirty(ui_dirty_queue_t *queue, ui_dirty_t dirty) {
    if (!dirty.changed || dirty.rect.width <= 0 || dirty.rect.height <= 0) {
        return;
    }
    dirty.rect = ui_align_blit_rect(dirty.rect, UI_WIDTH, UI_HEIGHT);
    if (dirty.rect.width <= 0 || dirty.rect.height <= 0) {
        return;
    }
    if (dirty.full) {
        queue->entries[0] = (ui_dirty_t){true, true, {0, 0, UI_WIDTH, UI_HEIGHT}};
        queue->count = 1u;
        return;
    }
    for (uint8_t i = 0u; i < queue->count; ++i) {
        if (queue->entries[i].full) {
            return;
        }
        if (rects_touch(queue->entries[i].rect, dirty.rect)) {
            queue->entries[i].rect = rect_union(queue->entries[i].rect, dirty.rect);
            queue->entries[i].rect =
                ui_align_blit_rect(queue->entries[i].rect, UI_WIDTH, UI_HEIGHT);
            return;
        }
    }
    if (queue->count < UI_DIRTY_CAPACITY) {
        queue->entries[queue->count++] = dirty;
        return;
    }
    ui_rect_t combined = dirty.rect;
    for (uint8_t i = 0u; i < queue->count; ++i) {
        combined = rect_union(combined, queue->entries[i].rect);
    }
    queue->entries[0] =
        (ui_dirty_t){true, false, ui_align_blit_rect(combined, UI_WIDTH, UI_HEIGHT)};
    queue->count = 1u;
}

static bool pop_dirty(ui_dirty_queue_t *queue, ui_dirty_t *dirty) {
    if (queue->count == 0u) {
        return false;
    }
    *dirty = queue->entries[0];
    --queue->count;
    for (uint8_t i = 0u; i < queue->count; ++i) {
        queue->entries[i] = queue->entries[i + 1u];
    }
    return true;
}

static ui_library_state_t library_state(const storage_t *storage) {
    switch (storage->state) {
    case STORAGE_STATE_NO_CARD:
        return UI_LIBRARY_NO_CARD;
    case STORAGE_STATE_READY:
        return UI_LIBRARY_READY;
    case STORAGE_STATE_SCANNING:
    case STORAGE_STATE_LOADING:
        return UI_LIBRARY_LOADING;
    case STORAGE_STATE_EMPTY:
        return UI_LIBRARY_EMPTY;
    default:
        return UI_LIBRARY_ERROR;
    }
}

static ui_model_t read_ui_model(const ui_runtime_config_t *config, const board_status_t *board,
                                uint32_t max_gui_slice_us, const spc_snapshot_t *snapshot,
                                const aram_view_t *aram_view) {
    storage_t *storage = config->storage;
    const player_status_t status = player_status_read(config->player_status);
    const i2s_output_stats_t audio = i2s_output_get_stats();
    storage_observe_played_generation(storage, audio.generation);
    const bool embedded = config->embedded_size != 0u && status.has_track &&
                          storage_should_show_embedded(storage->card_detected_at_boot);
    const uint32_t animation_ms = to_ms_since_boot(get_absolute_time());
    const display_stats_t display_stats = display_get_stats();
    return (ui_model_t){
        .player_ready = status.ready,
        .has_track = status.has_track,
        .player_mode = (uint8_t)status.mode,
        .audio_ready = atomic_load_explicit(config->audio_ready, memory_order_acquire),
        .paused = audio.generation != 0u ? audio.paused : status.paused,
        .touch_ready = board->touch_ready,
        .volume_step = audio.generation != 0u ? audio.volume_step : status.volume_step,
        .sys_clock_hz = board->sys_clock_hz,
        .generation = audio.generation,
        .silence_frames = audio.silence_frames,
        .underrun_blocks = audio.underrun_blocks,
        .late_dma_blocks = audio.late_dma_blocks,
        .dropped_snapshots = audio.dropped_snapshots,
        .dropped_visual_snapshots = snapshot == NULL ? 0u : snapshot->dropped_publications,
        .min_buffered_frames = audio.min_buffered_frames,
        .max_render_us = audio.max_render_us,
        .max_gui_slice_us = max_gui_slice_us,
        .display_last_transfer_us = display_stats.last_transfer_us,
        .display_max_transfer_us = display_stats.max_transfer_us,
        .track_frames = audio.track_frames,
        .peak_left = audio.peak_left,
        .peak_right = audio.peak_right,
        .metadata = embedded                     ? config->embedded_metadata
                    : storage->has_current_track ? &storage->active_metadata
                                                 : NULL,
        .spc_snapshot = snapshot,
        .snapshot_sequence = snapshot == NULL ? 0u : snapshot->sequence,
        .aram_view = aram_view,
        .aram_sequence = aram_view == NULL ? 0u : aram_view->sequence,
        .source_file = embedded                     ? config->embedded_basename
                       : storage->has_current_track ? storage->current_filename
                                                    : NULL,
        .source_game = embedded                     ? NULL
                       : storage->has_current_track ? storage->current_folder
                                                    : NULL,
        .source_embedded = embedded,
        .animation_ms = animation_ms,
        .measured_aram_hz_x10 = display_stats.measured_aram_hz_x10,
        .visualizer_hz = visual_pipeline_requested_hz(config->visuals),
        .library =
            {
                .catalog = &storage->catalog,
                .error = storage->error,
                .current_folder = storage->current_folder,
                .current_filename = storage->current_filename,
                .state = library_state(storage),
                .has_current_track = embedded || storage->has_current_track,
                .show_embedded = config->embedded_size != 0u &&
                                 storage_should_show_embedded(storage->card_detected_at_boot),
                .animation_ms = animation_ms,
            },
    };
}

static void queue_model_changes(ui_dirty_queue_t *queue, const ui_t *ui,
                                const ui_model_t *old_model, const ui_model_t *new_model) {
    const bool storage_changed =
        old_model->library.state != new_model->library.state ||
        old_model->library.has_current_track != new_model->library.has_current_track ||
        old_model->metadata != new_model->metadata ||
        old_model->generation != new_model->generation;
    if (storage_changed) {
        queue_dirty(queue, (ui_dirty_t){true, true, {0, 0, UI_WIDTH, UI_HEIGHT}});
        return;
    }
    if (old_model->player_ready != new_model->player_ready ||
        old_model->has_track != new_model->has_track ||
        old_model->player_mode != new_model->player_mode ||
        old_model->paused != new_model->paused) {
        queue_dirty(queue, (ui_dirty_t){true, false, {0, 64, 326, 30}});
    }
    if (old_model->generation != new_model->generation ||
        old_model->aram_sequence != new_model->aram_sequence) {
        if (ui->screen == UI_SCREEN_PLAYER) {
            queue_dirty(queue, (ui_dirty_t){true, false, {12, 124, 260, 260}});
        } else if (ui->screen == UI_SCREEN_ARAM) {
            queue_dirty(queue, (ui_dirty_t){true, false, UI_ARAM_LARGE_MAP});
        }
    }
    if (old_model->paused != new_model->paused || old_model->has_track != new_model->has_track) {
        queue_dirty(queue, (ui_dirty_t){true, false, UI_RECT_PLAY_PAUSE});
    }
    if (old_model->volume_step != new_model->volume_step) {
        if (ui->screen == UI_SCREEN_PLAYER) {
            queue_dirty(queue, (ui_dirty_t){true, false, {0, 408, 450, 64}});
        } else if (ui->screen == UI_SCREEN_VOLUME) {
            queue_dirty(queue, (ui_dirty_t){true, false, {0, 104, 450, 208}});
        }
    }
    const bool playback_changed = old_model->track_frames != new_model->track_frames ||
                                  old_model->peak_left != new_model->peak_left ||
                                  old_model->peak_right != new_model->peak_right;
    if (playback_changed && ui->screen == UI_SCREEN_PLAYER) {
        queue_dirty(queue, (ui_dirty_t){true, false, {0, 408, 450, 64}});
    } else if (playback_changed && ui->screen == UI_SCREEN_TRACK) {
        queue_dirty(queue, (ui_dirty_t){true, false, {0, 260, 450, 40}});
    }
    const bool diagnostics_changed =
        old_model->audio_ready != new_model->audio_ready ||
        old_model->silence_frames != new_model->silence_frames ||
        old_model->underrun_blocks != new_model->underrun_blocks ||
        old_model->late_dma_blocks != new_model->late_dma_blocks ||
        old_model->dropped_snapshots != new_model->dropped_snapshots ||
        old_model->dropped_visual_snapshots != new_model->dropped_visual_snapshots ||
        old_model->min_buffered_frames != new_model->min_buffered_frames ||
        old_model->max_render_us != new_model->max_render_us ||
        old_model->max_gui_slice_us != new_model->max_gui_slice_us ||
        old_model->measured_aram_hz_x10 != new_model->measured_aram_hz_x10;
    if (diagnostics_changed && ui->screen == UI_SCREEN_SETTINGS) {
        queue_dirty(queue, (ui_dirty_t){true, false, {0, 304, 450, 174}});
    }
    if (old_model->visualizer_hz != new_model->visualizer_hz && ui->screen == UI_SCREEN_SETTINGS) {
        queue_dirty(queue,
                    (ui_dirty_t){true, false,
                                 ui_target_rect(UI_SCREEN_SETTINGS, UI_TARGET_VISUALIZER_RATE)});
    }
    if (old_model->snapshot_sequence != new_model->snapshot_sequence) {
        if (ui->screen == UI_SCREEN_PLAYER) {
            queue_dirty(queue, (ui_dirty_t){true, false, {280, 124, 170, 256}});
        } else if (ui->screen == UI_SCREEN_VOICES && new_model->snapshot_sequence % 3u != 0u) {
            queue_dirty(queue, (ui_dirty_t){true, false, {12, 210, 426, 12}});
            queue_dirty(queue, (ui_dirty_t){true, false, {12, 346, 426, 12}});
        } else if ((ui->screen == UI_SCREEN_VOICES || ui->screen == UI_SCREEN_VOICE_DETAIL ||
                    ui->screen == UI_SCREEN_DSP) &&
                   new_model->snapshot_sequence % 3u == 0u) {
            queue_dirty(queue, (ui_dirty_t){true, false, {0, 100, 450, 380}});
        }
    }
}

static void start_next_ui_transfer(ui_dirty_queue_t *queue, const ui_t *ui, ui_canvas_t *canvas,
                                   uint32_t *max_gui_slice_us) {
    if (display_transfer_active()) {
        return;
    }
    ui_dirty_t dirty;
    if (!pop_dirty(queue, &dirty)) {
        return;
    }

    const uint32_t started_us = time_us_32();
    ui_render_dirty(ui, canvas, dirty);
    const uint8_t *source =
        canvas->pixels + (size_t)dirty.rect.y * canvas->stride_bytes + (uint16_t)dirty.rect.x / 2u;
    display_palette_region_t secondary_regions[DISPLAY_PALETTE_REGION_CAPACITY];
    uint8_t secondary_region_count = 0u;
    if (ui->screen == UI_SCREEN_ARAM && ui->aram_data_mode) {
        secondary_regions[secondary_region_count++] = (display_palette_region_t){
            (uint16_t)UI_ARAM_DATA_SCALE.x, (uint16_t)UI_ARAM_DATA_SCALE.y,
            (uint16_t)UI_ARAM_DATA_SCALE.width, (uint16_t)UI_ARAM_DATA_SCALE.height};
        if (ui_aram_data_ready(ui)) {
            secondary_regions[secondary_region_count++] = (display_palette_region_t){
                (uint16_t)UI_ARAM_LARGE_MAP.x, (uint16_t)UI_ARAM_LARGE_MAP.y,
                (uint16_t)UI_ARAM_LARGE_MAP.width, (uint16_t)UI_ARAM_LARGE_MAP.height};
        }
    }
    const bool started =
        display_begin_indexed4((uint16_t)dirty.rect.x, (uint16_t)dirty.rect.y,
                               (uint16_t)dirty.rect.width, (uint16_t)dirty.rect.height, source,
                               canvas->stride_bytes, secondary_regions, secondary_region_count);
    const bool activity_ready = ui->model.has_track && !ui->aram_data_mode &&
                                ui->model.aram_view != NULL && ui->model.aram_view->valid &&
                                ui->model.aram_view->kind == ARAM_VIEW_ACTIVITY &&
                                ui->model.aram_view->generation == ui->model.generation;
    const bool map_ready = activity_ready || ui_aram_data_ready(ui);
    if (!started) {
        queue_dirty(queue, dirty);
    } else if (ui->screen == UI_SCREEN_ARAM && map_ready && dirty.rect.x <= UI_ARAM_LARGE_MAP.x &&
               dirty.rect.y <= UI_ARAM_LARGE_MAP.y &&
               (int32_t)dirty.rect.x + dirty.rect.width >=
                   (int32_t)UI_ARAM_LARGE_MAP.x + UI_ARAM_LARGE_MAP.width &&
               (int32_t)dirty.rect.y + dirty.rect.height >=
                   (int32_t)UI_ARAM_LARGE_MAP.y + UI_ARAM_LARGE_MAP.height) {
        display_mark_current_transfer_aram();
    }
    const uint32_t elapsed_us = time_us_32() - started_us;
    if (elapsed_us > *max_gui_slice_us) {
        *max_gui_slice_us = elapsed_us;
    }
}

static void service_visible_titles(storage_t *storage, ui_t *ui, ui_dirty_queue_t *dirty_queue) {
    visible_title_t result;
    if (storage_take_visible_title(storage, &result)) {
        queue_dirty(dirty_queue, ui_accept_visible_title(ui, &result));
    }
    uint32_t page_revision = 0u;
    spc_track_ref_t tracks[3];
    uint8_t count = 0u;
    if (ui_take_visible_title_request(ui, &page_revision, tracks, &count)) {
        (void)storage_request_visible_titles(storage, page_revision, tracks, count);
    }
}

_Noreturn void ui_runtime_run(const ui_runtime_config_t *config) {
    storage_t *storage = config->storage;
    visual_pipeline_t *visuals = config->visuals;
    storage_begin(storage);

    const board_result_t ui_result = board_ui_init();
    const board_status_t board = board_get_status();
    printf("UI display=%u touch=%u PSRAM(optional)=%u init=%d\n", board.display_ready,
           board.touch_ready, board.psram_ready, ui_result);
    if (!board.display_ready) {
        while (true) {
            sleep_ms(UI_DISPLAY_RETRY_MS);
        }
    }

    ui_canvas_t canvas = {
        .pixels = ui_framebuffer,
        .width = UI_WIDTH,
        .height = UI_HEIGHT,
        .stride_bytes = UI_INDEXED4_STRIDE(UI_WIDTH),
    };
    uint32_t max_gui_slice_us = 0u;
    uint32_t aram_revision = 0u;
    aram_view_t ui_aram_view;
    aram_view_init(&ui_aram_view);
    aram_activity_snapshot_t aram_snapshot = {0};
    if (aram_activity_acquire(&visuals->activity, &aram_snapshot)) {
        aram_view_apply(&ui_aram_view, &aram_snapshot);
        ui_aram_view.sequence = ++aram_revision;
        aram_activity_release(&visuals->activity, &aram_snapshot);
    }
    aram_data_snapshot_t data_snapshot = {0};
    spc_snapshot_t visual_snapshot = {0};
    const spc_snapshot_t *visual_snapshot_ptr =
        spc_snapshot_queue_pop_latest(&visuals->snapshots, &visual_snapshot) ? &visual_snapshot
                                                                             : NULL;
    ui_model_t model =
        read_ui_model(config, &board, max_gui_slice_us, visual_snapshot_ptr, &ui_aram_view);
    ui_t ui;
    ui_init(&ui, &model);
    ui_dirty_queue_t dirty_queue = {0};
    queue_dirty(&dirty_queue, (ui_dirty_t){true, true, {0, 0, UI_WIDTH, UI_HEIGHT}});
    display_set_palette(ui_palette_rgb565);
    display_set_secondary_palette(ui_aram_data_palette_rgb565);
    start_next_ui_transfer(&dirty_queue, &ui, &canvas, &max_gui_slice_us);

    uint8_t model_hz = visual_pipeline_requested_hz(visuals);
    uint64_t next_model_update_us = time_us_64() + UI_TIMEBASE_US / model_hz;
    uint32_t model_deadline_remainder = 0u;
    absolute_time_t next_touch_poll = get_absolute_time();
    while (true) {
        const uint32_t slice_started_us = time_us_32();
        if (display_poll() == DISPLAY_TRANSFER_ERROR) {
            queue_dirty(&dirty_queue, (ui_dirty_t){true, true, {0, 0, UI_WIDTH, UI_HEIGHT}});
        }
        storage_poll(storage);
        service_visible_titles(storage, &ui, &dirty_queue);

        const uint64_t now_us = time_us_64();
        if (now_us >= next_model_update_us) {
            if (aram_activity_acquire(&visuals->activity, &aram_snapshot)) {
                if (!ui.aram_data_mode) {
                    aram_view_apply(&ui_aram_view, &aram_snapshot);
                    ui_aram_view.sequence = ++aram_revision;
                }
                aram_activity_release(&visuals->activity, &aram_snapshot);
            }
            if (aram_data_acquire(&visuals->data, &data_snapshot)) {
                const uint32_t current_generation =
                    player_status_read(config->player_status).generation;
                if (ui.screen == UI_SCREEN_ARAM && ui.aram_data_mode &&
                    data_snapshot.request == ui.aram_mode_request &&
                    data_snapshot.generation == current_generation) {
                    aram_view_apply_data(&ui_aram_view, &data_snapshot);
                    ui_aram_view.sequence = ++aram_revision;
                }
                aram_data_release(&visuals->data, &data_snapshot);
            }
            if (spc_snapshot_queue_pop_latest(&visuals->snapshots, &visual_snapshot)) {
                visual_snapshot_ptr = &visual_snapshot;
            }
            const ui_model_t next_model =
                read_ui_model(config, &board, max_gui_slice_us, visual_snapshot_ptr, &ui_aram_view);
            queue_model_changes(&dirty_queue, &ui, &model, &next_model);
            const bool rejected_before = ui.command_rejected;
            model = next_model;
            (void)ui_set_model(&ui, &model);
            queue_dirty(&dirty_queue, ui_update_animation(&ui, model.animation_ms));
            service_visible_titles(storage, &ui, &dirty_queue);
            if (rejected_before != ui.command_rejected) {
                queue_dirty(&dirty_queue, (ui_dirty_t){true, false, {0, 64, 326, 30}});
            }
            do {
                next_model_update_us += UI_TIMEBASE_US / model_hz;
                model_deadline_remainder += UI_TIMEBASE_US % model_hz;
                if (model_deadline_remainder >= model_hz) {
                    ++next_model_update_us;
                    model_deadline_remainder -= model_hz;
                }
            } while (next_model_update_us <= now_us);
        }

        if (time_reached(next_touch_poll)) {
            uint16_t x = 0u;
            uint16_t y = 0u;
            bool pressed = false;
            if (board_touch_read(&x, &y, &pressed)) {
                const bool rejected_before = ui.command_rejected;
                const uint16_t previous_folder = ui.library.selected_folder;
                const uint16_t previous_page = ui.library.page;
                const bool previous_tracks_visible = ui.library.tracks_visible;
                queue_dirty(&dirty_queue,
                            ui_handle_touch(&ui, x, y, pressed, config->player_commands));

                uint16_t requested_folder = 0u;
                if (ui_take_folder_request(&ui, &requested_folder)) {
                    ui.command_rejected = !storage_open_folder(storage, requested_folder);
                    if (ui.command_rejected) {
                        /* Keep the visible folder label paired with its existing catalog. */
                        ui.library.selected_folder = previous_folder;
                        ui.library.page = previous_page;
                        ui.library.tracks_visible = previous_tracks_visible;
                    }
                    queue_dirty(&dirty_queue,
                                (ui_dirty_t){true, true, {0, 0, UI_WIDTH, UI_HEIGHT}});
                }
                spc_track_ref_t requested_track = {0};
                if (ui_take_track_request(&ui, &requested_track)) {
                    ui.command_rejected = !storage_request_track(storage, requested_track);
                    queue_dirty(&dirty_queue,
                                (ui_dirty_t){true, true, {0, 0, UI_WIDTH, UI_HEIGHT}});
                }
                if (ui_take_restart_request(&ui)) {
                    ui.command_rejected = !storage_request_restart(storage);
                }
                uint8_t requested_hz = 0u;
                if (ui_take_visual_rate_request(&ui, &requested_hz)) {
                    visual_pipeline_set_requested_hz(visuals, requested_hz);
                    model_hz = visual_pipeline_requested_hz(visuals);
                    model_deadline_remainder = 0u;
                    next_model_update_us = time_us_64() + UI_TIMEBASE_US / model_hz;
                }
                uint32_t aram_request = 0u;
                if (ui_take_aram_mode_request(&ui, &aram_request)) {
                    aram_view_invalidate(&ui_aram_view);
                    visual_pipeline_set_aram_request(visuals, aram_request);
                }
                if (rejected_before != ui.command_rejected) {
                    queue_dirty(&dirty_queue, (ui_dirty_t){true, false, {0, 64, 326, 30}});
                }
                service_visible_titles(storage, &ui, &dirty_queue);
            }
            next_touch_poll = make_timeout_time_ms(UI_TOUCH_POLL_MS);
        }

        start_next_ui_transfer(&dirty_queue, &ui, &canvas, &max_gui_slice_us);
        const uint32_t slice_us = time_us_32() - slice_started_us;
        if (slice_us > max_gui_slice_us) {
            max_gui_slice_us = slice_us;
        }
        if (!display_transfer_active() && dirty_queue.count == 0u) {
            sleep_ms(1u);
        } else {
            tight_loop_contents();
        }
    }
}
