#include "ui_pages.h"

#include <stdio.h>

#include "audio/pcm_format.h"
#include "ui_marquee.h"

static const char *metadata_field(const char *field, const char *fallback) {
    return field != NULL && field[0] != '\0' ? field : fallback;
}

const char *ui_track_title(const ui_t *ui) {
    const char *fallback = ui->model.source_embedded       ? "EMBEDDED TRACK"
                           : ui->model.source_file == NULL ? "NO TRACK"
                                                           : ui->model.source_file;
    return ui->model.metadata == NULL ? fallback
                                      : metadata_field(ui->model.metadata->title, fallback);
}

const char *ui_game_title(const ui_t *ui) {
    const char *fallback = ui->model.source_embedded       ? "UNKNOWN GAME"
                           : ui->model.source_game == NULL ? "SD CARD"
                                                           : ui->model.source_game;
    return ui->model.metadata == NULL ? fallback
                                      : metadata_field(ui->model.metadata->game, fallback);
}

void ui_view_menu_draw(const ui_t *ui, ui_canvas_t *canvas) {
    static const char *labels[6] = {"ARAM", "VOICES", "DSP", "TRACK", "LIBRARY", "SETTINGS"};
    ui_draw_text(canvas, 12, 104, "SELECT VIEW", 2u, UI_COLOR_TEXT);
    for (uint32_t i = 0; i < 6u; ++i) {
        const ui_target_t target = (ui_target_t)(UI_TARGET_ARAM + i);
        ui_draw_button(canvas, ui_target_rect(UI_SCREEN_VIEW_MENU, target), labels[i], false,
                       ui->pressed_target == target);
    }
}

static void draw_track_time(const ui_t *ui, ui_canvas_t *canvas) {
    const uint64_t seconds = ui->model.track_frames / PCM_SAMPLE_RATE;
    const uint32_t minutes = (uint32_t)(seconds / 60u);
    const uint32_t remaining = (uint32_t)(seconds % 60u);
    char line[32];
    snprintf(line, sizeof(line), "TIME  %lu:%02lu / INF", (unsigned long)minutes,
             (unsigned long)remaining);
    ui_draw_text(canvas, 12, 268, line, 2u, UI_COLOR_TEXT);
}

void ui_track_draw(const ui_t *ui, ui_canvas_t *canvas) {
    const spc_metadata_t *metadata = ui->model.metadata;
    const char *artist = metadata == NULL ? "UNKNOWN" : metadata_field(metadata->artist, "UNKNOWN");
    const char *dumper = metadata == NULL ? "UNKNOWN" : metadata_field(metadata->dumper, "UNKNOWN");
    const char *comments = metadata == NULL ? "" : metadata_field(metadata->comments, "");
    ui_draw_text(canvas, 12, 108, "TRACK", 2u, UI_COLOR_TEXT);
    ui_draw_text_marquee(canvas, (ui_rect_t){12, 134, 426, 24}, ui_track_title(ui), 3u,
                         UI_COLOR_ACCENT, ui->track_offset);
    ui_draw_text_clipped(canvas, (ui_rect_t){12, 170, 426, 16}, ui_game_title(ui), 2u,
                         UI_COLOR_TEXT);
    char line[48];
    snprintf(line, sizeof(line), "ARTIST  %s", artist);
    ui_draw_text_clipped(canvas, (ui_rect_t){12, 204, 426, 16}, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "DUMPER  %s", dumper);
    ui_draw_text_clipped(canvas, (ui_rect_t){12, 232, 426, 16}, line, 2u, UI_COLOR_SECONDARY);
    draw_track_time(ui, canvas);
    char file_line[80];
    snprintf(file_line, sizeof(file_line), "FILE  %s",
             ui->model.source_file == NULL ? "UNKNOWN" : ui->model.source_file);
    ui_draw_text_clipped(canvas, (ui_rect_t){12, 300, 426, 16}, file_line, 2u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 12, 328, ui->model.source_embedded ? "SOURCE  EMBEDDED" : "SOURCE  SD", 2u,
                 UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 12, 368, "COMMENTS", 2u, UI_COLOR_TEXT);
    ui_draw_text_clipped(canvas, (ui_rect_t){12, 396, 426, 16}, comments, 2u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 12, 438, "PLAYBACK MODE  MANUAL", 2u, UI_COLOR_ACCENT);
}

void ui_settings_draw(const ui_t *ui, ui_canvas_t *canvas) {
    ui_draw_text(canvas, 12, 116, "SETTINGS", 3u, UI_COLOR_TEXT);
    ui_draw_text(canvas, 12, 174, "BRIGHTNESS  60%", 2u, UI_COLOR_SECONDARY);
    const ui_rect_t visualizer = ui_target_rect(UI_SCREEN_SETTINGS, UI_TARGET_VISUALIZER_RATE);
    const bool visualizer_pressed = ui->pressed_target == UI_TARGET_VISUALIZER_RATE;
    ui_draw_fill(canvas, visualizer, visualizer_pressed ? UI_COLOR_ACCENT : UI_COLOR_PANEL);
    ui_draw_outline(canvas, visualizer, UI_COLOR_BORDER, 1u);
    char visualizer_line[32];
    snprintf(visualizer_line, sizeof(visualizer_line), "VISUALIZER  %u HZ",
             (unsigned)ui->model.visualizer_hz);
    ui_draw_text(canvas, 20, 210, visualizer_line, 2u,
                 visualizer_pressed ? UI_COLOR_BACKGROUND : UI_COLOR_SECONDARY);
    char clock_line[32];
    snprintf(clock_line, sizeof(clock_line), "CLOCK  %lu MHZ",
             (unsigned long)(ui->model.sys_clock_hz / 1000000u));
    ui_draw_text(canvas, 12, 242, clock_line, 2u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 12, 270, "DIAGNOSTICS", 3u, UI_COLOR_TEXT);
    char line[64];
    snprintf(line, sizeof(line), "AUDIO  %s", ui->model.audio_ready ? "RUNNING" : "FAILED");
    ui_draw_text(canvas, 12, 320, line, 2u,
                 ui->model.audio_ready ? UI_COLOR_ACCENT : UI_COLOR_ERROR);
    snprintf(line, sizeof(line), "SILENCE FRAMES  %lu", (unsigned long)ui->model.silence_frames);
    ui_draw_text(canvas, 12, 354, line, 2u,
                 ui->model.silence_frames == 0u ? UI_COLOR_SECONDARY : UI_COLOR_ERROR);
    snprintf(line, sizeof(line), "UNDERRUN BLOCKS  %lu", (unsigned long)ui->model.underrun_blocks);
    ui_draw_text(canvas, 12, 382, line, 2u,
                 ui->model.underrun_blocks == 0u ? UI_COLOR_SECONDARY : UI_COLOR_ERROR);
    snprintf(line, sizeof(line), "DMA %lu A-DROP %lu V-DROP %lu",
             (unsigned long)ui->model.late_dma_blocks, (unsigned long)ui->model.dropped_snapshots,
             (unsigned long)ui->model.dropped_visual_snapshots);
    ui_draw_text(canvas, 12, 410, line, 2u,
                 ui->model.late_dma_blocks == 0u ? UI_COLOR_SECONDARY : UI_COLOR_ERROR);
    snprintf(line, sizeof(line), "BUF MIN %lu  RENDER %luUS",
             (unsigned long)ui->model.min_buffered_frames, (unsigned long)ui->model.max_render_us);
    ui_draw_text(canvas, 12, 438, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "GUI %luUS MAP %u.%uHZ", (unsigned long)ui->model.max_gui_slice_us,
             (unsigned)(ui->model.measured_aram_hz_x10 / 10u),
             (unsigned)(ui->model.measured_aram_hz_x10 % 10u));
    ui_draw_text(canvas, 12, 466, line, 2u, UI_COLOR_SECONDARY);
}

void ui_volume_draw(const ui_t *ui, ui_canvas_t *canvas) {
    ui_draw_text(canvas, 12, 116, "VOLUME", 3u, UI_COLOR_TEXT);
    char value[8];
    snprintf(value, sizeof(value), "%u%%", (unsigned)ui->model.volume_step * 100u / 8u);
    ui_draw_text(canvas, 170, 122, value, 4u, UI_COLOR_ACCENT);
    ui_draw_button(canvas, ui_target_rect(UI_SCREEN_VOLUME, UI_TARGET_VOLUME_DOWN), "-", false,
                   ui->pressed_target == UI_TARGET_VOLUME_DOWN);
    ui_draw_button(canvas, ui_target_rect(UI_SCREEN_VOLUME, UI_TARGET_VOLUME_UP), "+", false,
                   ui->pressed_target == UI_TARGET_VOLUME_UP);
    ui_draw_text(canvas, 54, 306, "8 STEPS PLUS MUTE", 2u, UI_COLOR_SECONDARY);
}
