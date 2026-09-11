#include "ui_player.h"

#include <stdio.h>

#include "ui_aram.h"
#include "ui_voices.h"

static void format_elapsed(char *text, size_t capacity, uint64_t frames) {
    const uint64_t seconds = frames / 32000u;
    snprintf(text, capacity, "%lu:%02lu", (unsigned long)(seconds / 60u),
             (unsigned long)(seconds % 60u));
}

static void draw_status(const ui_t *ui, ui_canvas_t *canvas) {
    char elapsed[16];
    format_elapsed(elapsed, sizeof(elapsed), ui->model.track_frames);
    char status[40];
    snprintf(status, sizeof(status), "%s / INF     VOL %u%%", elapsed,
             (unsigned)ui->model.volume_step * 100u / 8u);
    ui_draw_text(canvas, 12, 414, status, 2u, UI_COLOR_TEXT);
    ui_draw_text(canvas, 12, 448, "L", 2u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 226, 448, "R", 2u, UI_COLOR_SECONDARY);
    const uint16_t left_width = (uint16_t)((uint32_t)ui->model.peak_left * 170u / 32768u);
    const uint16_t right_width = (uint16_t)((uint32_t)ui->model.peak_right * 170u / 32768u);
    ui_draw_fill(canvas, (ui_rect_t){32, 448, 170, 14}, UI_COLOR_INACTIVE);
    ui_draw_fill(canvas, (ui_rect_t){246, 448, 170, 14}, UI_COLOR_INACTIVE);
    ui_draw_fill(canvas, (ui_rect_t){32, 448, (int16_t)left_width, 14}, UI_COLOR_WRITE);
    ui_draw_fill(canvas, (ui_rect_t){246, 448, (int16_t)right_width, 14}, UI_COLOR_WRITE);
    ui_draw_outline(canvas, (ui_rect_t){32, 448, 170, 14}, UI_COLOR_BORDER, 1u);
    ui_draw_outline(canvas, (ui_rect_t){246, 448, 170, 14}, UI_COLOR_BORDER, 1u);
}

void ui_player_draw(const ui_t *ui, ui_canvas_t *canvas) {
    ui_draw_text(canvas, 12, 102, ui->aram_data_mode ? "ARAM DATA" : "ARAM ACTIVITY", 2u,
                 UI_COLOR_TEXT);
    ui_draw_text(canvas, 284, 102, "VOICES 1-8", 2u, UI_COLOR_TEXT);
    const ui_rect_t border = {12, 124, 260, 260};
    ui_draw_fill(canvas, UI_RECT_PLAYER_ARAM_MAP, UI_COLOR_INACTIVE);
    const bool activity_valid = ui->model.has_track && !ui->aram_data_mode &&
                                ui->model.aram_view != NULL && ui->model.aram_view->valid &&
                                ui->model.aram_view->kind == ARAM_VIEW_ACTIVITY &&
                                ui->model.aram_view->generation == ui->model.generation;
    const bool aram_valid = activity_valid || ui_aram_data_ready(ui);
    if (aram_valid) {
        aram_view_blit(ui->model.aram_view, canvas, UI_RECT_PLAYER_ARAM_MAP.x,
                       UI_RECT_PLAYER_ARAM_MAP.y);
    } else {
        ui_draw_text(canvas, ui->aram_data_mode ? 90 : 48, 278,
                     ui->aram_data_mode ? "DATA" : "ACTIVITY", 2u, UI_COLOR_SECONDARY);
        ui_draw_text(canvas, 42, 302, "UNAVAILABLE", 2u, UI_COLOR_SECONDARY);
    }
    ui_draw_outline(canvas, border,
                    ui->pressed_target == UI_TARGET_ARAM_MAP ? UI_COLOR_ACCENT : UI_COLOR_BORDER,
                    2u);
    const uint32_t validity = SPC_SNAPSHOT_VALID_DSP_REGISTERS | SPC_SNAPSHOT_VALID_VOICE_INTERNALS;
    const bool snapshot_valid = ui->model.spc_snapshot != NULL &&
                                ui->model.spc_snapshot->generation == ui->model.generation &&
                                (ui->model.spc_snapshot->validity & validity) == validity;
    ui_voices_draw_summary(canvas, ui->model.spc_snapshot, snapshot_valid);
    if (ui->aram_data_mode) {
        ui_draw_outline(canvas, (ui_rect_t){12, 386, 260, 16}, UI_COLOR_BORDER, 2u);
        for (uint8_t index = 0u; index < 16u; ++index) {
            ui_draw_fill(canvas, (ui_rect_t){(int16_t)(14 + index * 16u), 388, 16, 12}, index);
        }
        ui_draw_text(canvas, 12, 402, "00", 1u, UI_COLOR_SECONDARY);
        ui_draw_text(canvas, 134, 402, "80", 1u, UI_COLOR_SECONDARY);
        ui_draw_text(canvas, 258, 402, "FF", 1u, UI_COLOR_SECONDARY);
    } else {
        ui_draw_fill(canvas, (ui_rect_t){12, 388, 12, 12}, UI_COLOR_READ);
        ui_draw_text(canvas, 28, 386, "READ", 2u, UI_COLOR_SECONDARY);
        ui_draw_fill(canvas, (ui_rect_t){102, 388, 12, 12}, UI_COLOR_WRITE);
        ui_draw_text(canvas, 118, 386, "WRITE", 2u, UI_COLOR_SECONDARY);
        ui_draw_fill(canvas, (ui_rect_t){220, 388, 12, 12}, UI_COLOR_EXECUTE);
        ui_draw_text(canvas, 236, 386, "EXEC", 2u, UI_COLOR_SECONDARY);
    }
    draw_status(ui, canvas);
}
