#ifndef UI_DSP_H
#define UI_DSP_H

#include <stdbool.h>
#include <stdint.h>

#include "spc/spc_snapshot.h"
#include "ui_draw.h"
#include "ui_layout.h"

void ui_dsp_draw(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid, uint8_t page,
                 ui_target_t pressed_target);

#endif
