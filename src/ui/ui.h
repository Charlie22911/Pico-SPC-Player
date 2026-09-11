#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

#include "player/player_commands.h"
#include "spc/spc_metadata.h"
#include "spc/spc_snapshot.h"
#include "ui_draw.h"
#include "ui_library.h"
#include "ui_touch.h"
#include "visualizer/aram_view.h"

typedef struct {
    bool player_ready;
    bool has_track;
    uint8_t player_mode;
    bool audio_ready;
    bool paused;
    bool touch_ready;
    uint8_t volume_step;
    uint32_t sys_clock_hz;
    uint32_t generation;
    uint32_t silence_frames;
    uint32_t underrun_blocks;
    uint32_t late_dma_blocks;
    uint32_t dropped_snapshots;
    uint32_t dropped_visual_snapshots;
    uint32_t min_buffered_frames;
    uint32_t max_render_us;
    uint32_t max_gui_slice_us;
    uint32_t display_last_transfer_us;
    uint32_t display_max_transfer_us;
    uint64_t track_frames;
    uint16_t peak_left;
    uint16_t peak_right;
    const spc_metadata_t *metadata;
    const spc_snapshot_t *spc_snapshot;
    uint32_t snapshot_sequence;
    const aram_view_t *aram_view;
    uint32_t aram_sequence;
    const char *source_file;
    const char *source_game;
    bool source_embedded;
    uint32_t animation_ms;
    uint16_t measured_aram_hz_x10;
    uint8_t visualizer_hz;
    ui_library_model_t library;
} ui_model_t;

typedef struct {
    bool changed;
    bool full;
    ui_rect_t rect;
} ui_dirty_t;

typedef struct {
    ui_screen_t screen;
    ui_target_t pressed_target;
    ui_touch_t touch;
    ui_model_t model;
    bool command_rejected;
    uint8_t voice_bank;
    uint8_t voice_number;
    uint8_t dsp_page;
    ui_library_t library;
    bool folder_request_pending;
    uint16_t requested_folder;
    bool track_request_pending;
    spc_track_ref_t requested_track;
    bool restart_request_pending;
    uint32_t header_epoch_ms;
    uint32_t track_epoch_ms;
    int16_t header_offset;
    int16_t track_offset;
    char marquee_title[SPC_LFN_CAPACITY];
    bool visual_rate_request_pending;
    uint8_t requested_visual_rate;
    bool aram_data_mode;
    bool aram_mode_request_pending;
    uint32_t aram_mode_serial;
    uint32_t aram_mode_request;
} ui_t;

void ui_init(ui_t *ui, const ui_model_t *model);
bool ui_set_model(ui_t *ui, const ui_model_t *model);
void ui_render(const ui_t *ui, ui_canvas_t *canvas);
void ui_render_dirty(const ui_t *ui, ui_canvas_t *canvas, ui_dirty_t dirty);
ui_dirty_t ui_handle_touch(ui_t *ui, uint16_t x, uint16_t y, bool pressed,
                           player_command_queue_t *commands);
bool ui_take_folder_request(ui_t *ui, uint16_t *folder_index);
bool ui_take_track_request(ui_t *ui, spc_track_ref_t *track);
bool ui_take_restart_request(ui_t *ui);
bool ui_take_visible_title_request(ui_t *ui, uint32_t *page_revision, spc_track_ref_t *tracks,
                                   uint8_t *count);
ui_dirty_t ui_accept_visible_title(ui_t *ui, const visible_title_t *title);
ui_dirty_t ui_update_animation(ui_t *ui, uint32_t animation_ms);
bool ui_take_visual_rate_request(ui_t *ui, uint8_t *requested_hz);
bool ui_take_aram_mode_request(ui_t *ui, uint32_t *request);

#endif
