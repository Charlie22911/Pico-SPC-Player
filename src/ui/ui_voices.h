#ifndef UI_VOICES_H
#define UI_VOICES_H

#include <stdbool.h>
#include <stdint.h>

#include "spc/spc_snapshot.h"
#include "ui_draw.h"
#include "ui_layout.h"

void ui_voices_draw_summary(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid);
void ui_voices_draw_bank(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid,
                         uint8_t bank, ui_target_t pressed_target);
void ui_voices_draw_detail(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid,
                           uint8_t voice);

#endif
