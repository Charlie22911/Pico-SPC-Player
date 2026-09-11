#ifndef UI_ARAM_H
#define UI_ARAM_H

#include "ui.h"

extern const ui_rect_t UI_ARAM_LARGE_BORDER;
extern const ui_rect_t UI_ARAM_LARGE_MAP;
extern const ui_rect_t UI_ARAM_LARGE_LEGEND;
extern const ui_rect_t UI_ARAM_DATA_SCALE;
extern const uint16_t ui_aram_data_palette_rgb565[16];

void ui_aram_draw(const ui_t *ui, ui_canvas_t *canvas);
bool ui_aram_data_ready(const ui_t *ui);

#endif
