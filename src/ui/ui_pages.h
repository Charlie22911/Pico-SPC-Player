#ifndef UI_PAGES_H
#define UI_PAGES_H

#include "ui.h"

const char *ui_track_title(const ui_t *ui);
const char *ui_game_title(const ui_t *ui);
void ui_view_menu_draw(const ui_t *ui, ui_canvas_t *canvas);
void ui_track_draw(const ui_t *ui, ui_canvas_t *canvas);
void ui_settings_draw(const ui_t *ui, ui_canvas_t *canvas);
void ui_volume_draw(const ui_t *ui, ui_canvas_t *canvas);

#endif
