#ifndef UI_DRAW_H
#define UI_DRAW_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_layout.h"

#define UI_INDEXED4_STRIDE(width) (((width) + 1u) / 2u)

typedef struct {
    uint8_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t stride_bytes;
    /* Zero dimensions mean unrestricted; negative dimensions mean empty. */
    ui_rect_t clip;
} ui_canvas_t;

enum {
    UI_COLOR_BACKGROUND = 0,
    UI_COLOR_PANEL = 1,
    UI_COLOR_BORDER = 2,
    UI_COLOR_TEXT = 3,
    UI_COLOR_SECONDARY = 4,
    UI_COLOR_ACCENT = 5,
    UI_COLOR_READ = 6,
    UI_COLOR_WRITE = 7,
    UI_COLOR_EXECUTE = 8,
    UI_COLOR_ERROR = 9,
    UI_COLOR_INACTIVE = 10,
    UI_PALETTE_SIZE = 16,
};

extern const uint16_t ui_palette_rgb565[UI_PALETTE_SIZE];

void ui_canvas_set_clip(ui_canvas_t *canvas, ui_rect_t clip);
void ui_canvas_reset_clip(ui_canvas_t *canvas);
uint8_t ui_canvas_get_pixel(const ui_canvas_t *canvas, uint16_t x, uint16_t y);
void ui_canvas_clear(ui_canvas_t *canvas, uint8_t color);
void ui_draw_fill(ui_canvas_t *canvas, ui_rect_t rect, uint8_t color);
void ui_draw_outline(ui_canvas_t *canvas, ui_rect_t rect, uint8_t color, uint16_t thickness);
void ui_draw_text(ui_canvas_t *canvas, int16_t x, int16_t y, const char *text, uint8_t scale,
                  uint8_t color);
void ui_draw_text_clipped(ui_canvas_t *canvas, ui_rect_t bounds, const char *text, uint8_t scale,
                          uint8_t color);
void ui_draw_button(ui_canvas_t *canvas, ui_rect_t rect, const char *label, bool selected,
                    bool pressed);

#endif
