#include "ui_draw.h"

#include "ui_font.h"

#include <stddef.h>
#include <string.h>

const uint16_t ui_palette_rgb565[UI_PALETTE_SIZE] = {
    0x0862, 0x10e4, 0x322a, 0xe79e, 0x9d98, 0x46de, 0x445f, 0x670a,
    0xfdea, 0xfb0d, 0x2146, 0x4fff, 0xc55f, 0xafea, 0xffff, 0x2965,
};

static ui_rect_t intersect(ui_rect_t a, ui_rect_t b) {
    int32_t x0 = a.x > b.x ? a.x : b.x;
    int32_t y0 = a.y > b.y ? a.y : b.y;
    int32_t ax1 = (int32_t)a.x + a.width;
    int32_t ay1 = (int32_t)a.y + a.height;
    int32_t bx1 = (int32_t)b.x + b.width;
    int32_t by1 = (int32_t)b.y + b.height;
    int32_t x1 = ax1 < bx1 ? ax1 : bx1;
    int32_t y1 = ay1 < by1 ? ay1 : by1;
    if (x1 < x0)
        x1 = x0;
    if (y1 < y0)
        y1 = y0;
    return (ui_rect_t){(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
}

static ui_rect_t clip_rect(const ui_canvas_t *canvas, ui_rect_t rect) {
    if (canvas->clip.width < 0 || canvas->clip.height < 0)
        return (ui_rect_t){0};
    const ui_rect_t bounds = {0, 0, (int16_t)canvas->width, (int16_t)canvas->height};
    rect = intersect(rect, bounds);
    if (canvas->clip.width > 0 && canvas->clip.height > 0) {
        rect = intersect(rect, canvas->clip);
    }
    return rect;
}

void ui_canvas_set_clip(ui_canvas_t *canvas, ui_rect_t clip) {
    if (canvas == NULL)
        return;
    canvas->clip = clip_rect(canvas, clip);
    if (canvas->clip.width <= 0 || canvas->clip.height <= 0)
        canvas->clip = (ui_rect_t){0, 0, -1, -1};
}

void ui_canvas_reset_clip(ui_canvas_t *canvas) {
    if (canvas != NULL)
        canvas->clip = (ui_rect_t){0};
}

uint8_t ui_canvas_get_pixel(const ui_canvas_t *canvas, uint16_t x, uint16_t y) {
    if (canvas == NULL || canvas->pixels == NULL || x >= canvas->width || y >= canvas->height) {
        return 0u;
    }
    const uint8_t packed = canvas->pixels[(size_t)y * canvas->stride_bytes + x / 2u];
    return (uint8_t)((x & 1u) == 0u ? packed >> 4u : packed & 0x0fu);
}

static void set_span(uint8_t *row, uint16_t x, uint16_t width, uint8_t color) {
    const uint8_t nibble = color & 0x0fu;
    if ((x & 1u) != 0u && width != 0u) {
        row[x / 2u] = (uint8_t)((row[x / 2u] & 0xf0u) | nibble);
        ++x;
        --width;
    }
    const uint16_t pairs = width / 2u;
    if (pairs != 0u) {
        memset(row + x / 2u, (int)(nibble << 4u | nibble), pairs);
        x = (uint16_t)(x + pairs * 2u);
        width = (uint16_t)(width - pairs * 2u);
    }
    if (width != 0u) {
        row[x / 2u] = (uint8_t)((row[x / 2u] & 0x0fu) | nibble << 4u);
    }
}

void ui_draw_fill(ui_canvas_t *canvas, ui_rect_t rect, uint8_t color) {
    if (canvas == NULL || canvas->pixels == NULL)
        return;
    rect = clip_rect(canvas, rect);
    if (rect.width <= 0 || rect.height <= 0)
        return;
    for (int32_t y = rect.y; y < (int32_t)rect.y + rect.height; ++y) {
        uint8_t *row = canvas->pixels + (size_t)y * canvas->stride_bytes;
        set_span(row, (uint16_t)rect.x, (uint16_t)rect.width, color);
    }
}

void ui_canvas_clear(ui_canvas_t *canvas, uint8_t color) {
    ui_draw_fill(canvas, (ui_rect_t){0, 0, (int16_t)canvas->width, (int16_t)canvas->height}, color);
}

void ui_draw_outline(ui_canvas_t *canvas, ui_rect_t rect, uint8_t color, uint16_t thickness) {
    if (thickness == 0u)
        return;
    ui_draw_fill(canvas, (ui_rect_t){rect.x, rect.y, rect.width, (int16_t)thickness}, color);
    ui_draw_fill(canvas,
                 (ui_rect_t){rect.x, (int16_t)(rect.y + rect.height - thickness), rect.width,
                             (int16_t)thickness},
                 color);
    ui_draw_fill(canvas, (ui_rect_t){rect.x, rect.y, (int16_t)thickness, rect.height}, color);
    ui_draw_fill(canvas,
                 (ui_rect_t){(int16_t)(rect.x + rect.width - thickness), rect.y, (int16_t)thickness,
                             rect.height},
                 color);
}

void ui_draw_text(ui_canvas_t *canvas, int16_t x, int16_t y, const char *text, uint8_t scale,
                  uint8_t color) {
    if (canvas == NULL || text == NULL || scale == 0u)
        return;
    const ui_rect_t visible =
        clip_rect(canvas, (ui_rect_t){0, 0, (int16_t)canvas->width, (int16_t)canvas->height});
    if (visible.width <= 0 || visible.height <= 0 || y >= (int32_t)visible.y + visible.height ||
        (int32_t)y + 7 * scale <= visible.y)
        return;
    int32_t pen_x = x;
    for (; *text != '\0'; ++text) {
        if (pen_x >= (int32_t)visible.x + visible.width)
            break;
        if (pen_x + 5 * scale <= visible.x) {
            pen_x += 6 * scale;
            continue;
        }
        const uint8_t *glyph = ui_font_glyph(*text);
        for (uint8_t row = 0; row < 7u; ++row) {
            for (uint8_t col = 0; col < 5u; ++col) {
                if ((glyph[row] & (1u << (4u - col))) != 0u) {
                    ui_draw_fill(canvas,
                                 (ui_rect_t){(int16_t)(pen_x + col * scale),
                                             (int16_t)(y + row * scale), scale, scale},
                                 color);
                }
            }
        }
        pen_x += 6 * scale;
    }
}

void ui_draw_text_clipped(ui_canvas_t *canvas, ui_rect_t bounds, const char *text, uint8_t scale,
                          uint8_t color) {
    if (canvas == NULL || text == NULL || scale == 0u || bounds.width <= 0 || bounds.height <= 0)
        return;
    const size_t length = strlen(text);
    const size_t cell_width = 6u * scale;
    size_t count = (size_t)bounds.width / cell_width;
    if (count > length)
        count = length;
    char clipped[48];
    if (count >= sizeof(clipped))
        count = sizeof(clipped) - 1u;
    memcpy(clipped, text, count);
    clipped[count] = '\0';
    if (count < length && count >= 3u) {
        clipped[count - 3u] = '.';
        clipped[count - 2u] = '.';
        clipped[count - 1u] = '.';
    }
    const ui_rect_t saved_clip = canvas->clip;
    ui_canvas_set_clip(canvas, bounds);
    ui_draw_text(canvas, bounds.x, bounds.y, clipped, scale, color);
    canvas->clip = saved_clip;
}

void ui_draw_button(ui_canvas_t *canvas, ui_rect_t rect, const char *label, bool selected,
                    bool pressed) {
    const uint8_t fill = pressed ? UI_COLOR_ACCENT : UI_COLOR_PANEL;
    const uint8_t border = selected ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    const uint8_t text = pressed ? UI_COLOR_BACKGROUND : UI_COLOR_TEXT;
    ui_draw_fill(canvas, rect, fill);
    ui_draw_outline(canvas, rect, border, 3u);
    const int32_t text_width = (int32_t)strlen(label) * 18 - 3;
    const int16_t x = (int16_t)(rect.x + (rect.width - text_width) / 2);
    const int16_t y = (int16_t)(rect.y + (rect.height - 21) / 2);
    ui_draw_text(canvas, x, y, label, 3u, text);
}
