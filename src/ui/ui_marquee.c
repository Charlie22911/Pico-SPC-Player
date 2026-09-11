#include "ui_marquee.h"

#include <stddef.h>
#include <string.h>

uint16_t ui_text_width(const char *text, uint8_t scale) {
    if (text == NULL || text[0] == '\0' || scale == 0u)
        return 0u;
    const size_t cells = strlen(text);
    const size_t width = cells * 6u * scale - scale;
    return width > UINT16_MAX ? UINT16_MAX : (uint16_t)width;
}

int16_t ui_marquee_offset(uint16_t text_width, uint16_t viewport_width, uint32_t elapsed_ms) {
    if (text_width <= viewport_width)
        return 0;
    const uint32_t overflow = (uint32_t)text_width - viewport_width;
    const uint32_t hold = 800u;
    const uint32_t travel = overflow * 1000u / 25u;
    const uint32_t cycle = 2u * (hold + travel);
    uint32_t t = elapsed_ms % cycle;
    if (t < hold)
        return 0;
    t -= hold;
    if (t < travel)
        return (int16_t)((uint64_t)t * overflow / travel);
    t -= travel;
    if (t < hold)
        return (int16_t)overflow;
    t -= hold;
    return (int16_t)(overflow - (uint64_t)t * overflow / travel);
}

void ui_draw_text_marquee(ui_canvas_t *canvas, ui_rect_t viewport, const char *text, uint8_t scale,
                          uint8_t color, int16_t offset) {
    if (canvas == NULL || text == NULL || viewport.width <= 0 || viewport.height <= 0)
        return;
    const ui_rect_t saved_clip = canvas->clip;
    ui_canvas_set_clip(canvas, viewport);
    ui_draw_text(canvas, (int16_t)(viewport.x - offset), viewport.y, text, scale, color);
    canvas->clip = saved_clip;
}
