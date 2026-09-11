#ifndef UI_MARQUEE_H
#define UI_MARQUEE_H

#include <stdint.h>

#include "ui_draw.h"

uint16_t ui_text_width(const char *text, uint8_t scale);
int16_t ui_marquee_offset(uint16_t text_width, uint16_t viewport_width, uint32_t elapsed_ms);
void ui_draw_text_marquee(ui_canvas_t *canvas, ui_rect_t viewport, const char *text, uint8_t scale,
                          uint8_t color, int16_t offset);

#endif
