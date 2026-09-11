#include "ui_aram.h"

const ui_rect_t UI_ARAM_LARGE_BORDER = {12, 124, 426, 314};
const ui_rect_t UI_ARAM_LARGE_MAP = {14, 126, 422, 310};
const ui_rect_t UI_ARAM_LARGE_LEGEND = {12, 444, 426, 32};
const ui_rect_t UI_ARAM_DATA_SCALE = {16, 444, 416, 12};

#define RGB565(r, g, b)                                                                            \
    ((uint16_t)((((uint32_t)(r) & 0xf8u) << 8u) | (((uint32_t)(g) & 0xfcu) << 3u) |                \
                ((uint32_t)(b) >> 3u)))

const uint16_t ui_aram_data_palette_rgb565[16] = {
    RGB565(0, 0, 0),      RGB565(64, 128, 255), RGB565(32, 176, 255), RGB565(0, 224, 255),
    RGB565(0, 255, 224),  RGB565(32, 255, 160), RGB565(96, 255, 96),  RGB565(176, 255, 48),
    RGB565(240, 255, 32), RGB565(255, 224, 32), RGB565(255, 176, 32), RGB565(255, 128, 48),
    RGB565(255, 80, 80),  RGB565(255, 64, 144), RGB565(255, 64, 208), RGB565(224, 96, 255),
};

static void legend_item(ui_canvas_t *canvas, int16_t x, uint8_t color, const char *text) {
    ui_draw_fill(canvas, (ui_rect_t){x, 452, 10, 10}, color);
    ui_draw_text(canvas, (int16_t)(x + 14), 449, text, 2u, UI_COLOR_SECONDARY);
}

bool ui_aram_data_ready(const ui_t *ui) {
    return ui != NULL && ui->model.has_track && ui->aram_data_mode && ui->model.aram_view != NULL &&
           ui->model.aram_view->valid && ui->model.aram_view->kind == ARAM_VIEW_DATA &&
           ui->model.aram_view->request == ui->aram_mode_request &&
           ui->model.aram_view->generation == ui->model.generation;
}

static void draw_data_scale(ui_canvas_t *canvas) {
    ui_draw_outline(canvas, (ui_rect_t){14, 442, 420, 16}, UI_COLOR_BORDER, 2u);
    for (uint8_t index = 0u; index < 16u; ++index) {
        ui_draw_fill(canvas, (ui_rect_t){(int16_t)(16 + index * 26u), 444, 26, 12}, index);
    }
    ui_draw_text(canvas, 16, 462, "00", 1u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 120, 462, "40", 1u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 224, 462, "80", 1u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 328, 462, "C0", 1u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 420, 462, "FF", 1u, UI_COLOR_SECONDARY);
}

void ui_aram_draw(const ui_t *ui, ui_canvas_t *canvas) {
    ui_draw_text(canvas, 12, 102, ui->aram_data_mode ? "ARAM DATA" : "ARAM ACTIVITY", 2u,
                 UI_COLOR_TEXT);
    ui_draw_text(canvas, ui->aram_data_mode ? 270 : 330, 104,
                 ui->aram_data_mode ? "TAP: ACTIVITY" : "TAP: DATA", 1u, UI_COLOR_SECONDARY);
    ui_draw_fill(canvas, UI_ARAM_LARGE_MAP, UI_COLOR_INACTIVE);
    const bool activity_valid = ui->model.has_track && !ui->aram_data_mode &&
                                ui->model.aram_view != NULL && ui->model.aram_view->valid &&
                                ui->model.aram_view->kind == ARAM_VIEW_ACTIVITY &&
                                ui->model.aram_view->generation == ui->model.generation;
    const bool valid = activity_valid || ui_aram_data_ready(ui);
    if (valid) {
        aram_view_blit_scaled(ui->model.aram_view, canvas, UI_ARAM_LARGE_MAP);
    } else {
        ui_draw_text(canvas, ui->aram_data_mode ? 144 : 126, 270,
                     ui->aram_data_mode ? "DATA UNAVAILABLE" : "ACTIVITY UNAVAILABLE", 2u,
                     UI_COLOR_SECONDARY);
    }
    ui_draw_outline(canvas, UI_ARAM_LARGE_BORDER,
                    ui->pressed_target == UI_TARGET_ARAM_MAP ? UI_COLOR_ACCENT : UI_COLOR_BORDER,
                    2u);
    if (ui->aram_data_mode) {
        draw_data_scale(canvas);
    } else {
        legend_item(canvas, 12, UI_COLOR_READ, "READ");
        legend_item(canvas, 104, UI_COLOR_WRITE, "WRITE");
        legend_item(canvas, 214, UI_COLOR_EXECUTE, "EXEC");
        legend_item(canvas, 304, 14u, "OVERLAP");
    }
}
