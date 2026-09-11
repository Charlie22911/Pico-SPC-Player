#include "ui_dsp.h"

#include <stdio.h>

enum {
    DSP_MVOLL = 0x0c,
    DSP_MVOLR = 0x1c,
    DSP_EVOLL = 0x2c,
    DSP_EVOLR = 0x3c,
    DSP_FLG = 0x6c,
    DSP_PMON = 0x2d,
    DSP_NON = 0x3d,
    DSP_EON = 0x4d,
    DSP_DIR = 0x5d,
    DSP_ESA = 0x6d,
    DSP_EDL = 0x7d,
    DSP_ENDX = 0x7c,
};

static void draw_mix_echo(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid) {
    ui_draw_text(canvas, 12, 104, "DSP MIX / ECHO", 2u, UI_COLOR_TEXT);
    if (!valid) {
        ui_draw_text(canvas, 84, 260, "DSP DATA UNAVAILABLE", 2u, UI_COLOR_SECONDARY);
        return;
    }
    const uint8_t *r = snapshot->dsp_registers;
    char line[48];
    snprintf(line, sizeof(line), "MASTER L %+d   R %+d", (int)(int8_t)r[DSP_MVOLL],
             (int)(int8_t)r[DSP_MVOLR]);
    ui_draw_text(canvas, 12, 138, line, 2u, UI_COLOR_ACCENT);
    snprintf(line, sizeof(line), "ECHO   L %+d   R %+d", (int)(int8_t)r[DSP_EVOLL],
             (int)(int8_t)r[DSP_EVOLR]);
    ui_draw_text(canvas, 12, 166, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "FLG %02X  NOISE RATE %02X", (unsigned)r[DSP_FLG],
             (unsigned)(r[DSP_FLG] & 0x1fu));
    ui_draw_text(canvas, 12, 200, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "PMON %02X  NON %02X", (unsigned)r[DSP_PMON],
             (unsigned)r[DSP_NON]);
    ui_draw_text(canvas, 12, 228, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "EON  %02X  ENDX %02X", (unsigned)r[DSP_EON],
             (unsigned)r[DSP_ENDX]);
    ui_draw_text(canvas, 12, 256, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "DIR $%04X  ECHO $%04X", (unsigned)r[DSP_DIR] << 8u,
             (unsigned)r[DSP_ESA] << 8u);
    ui_draw_text(canvas, 12, 284, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "EDL %X  SIZE %u BYTES", (unsigned)(r[DSP_EDL] & 0x0fu),
             (unsigned)(r[DSP_EDL] & 0x0fu) * 2048u);
    ui_draw_text(canvas, 12, 312, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "FIR %+d %+d %+d %+d", (int)(int8_t)r[0x0f], (int)(int8_t)r[0x1f],
             (int)(int8_t)r[0x2f], (int)(int8_t)r[0x3f]);
    ui_draw_text(canvas, 12, 340, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "    %+d %+d %+d %+d", (int)(int8_t)r[0x4f], (int)(int8_t)r[0x5f],
             (int)(int8_t)r[0x6f], (int)(int8_t)r[0x7f]);
    ui_draw_text(canvas, 12, 364, line, 2u, UI_COLOR_SECONDARY);
}

static void draw_registers(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid) {
    ui_draw_text(canvas, 12, 104, "DSP REGISTERS", 2u, UI_COLOR_TEXT);
    if (!valid) {
        ui_draw_text(canvas, 84, 260, "DSP DATA UNAVAILABLE", 2u, UI_COLOR_SECONDARY);
        return;
    }
    for (uint8_t column = 0u; column < 8u; ++column) {
        char label[2] = {(char)('0' + column), '\0'};
        ui_draw_text(canvas, (int16_t)(48 + column * 48u), 126, label, 1u, UI_COLOR_SECONDARY);
    }
    for (uint8_t row = 0u; row < 16u; ++row) {
        char label[4];
        snprintf(label, sizeof(label), "%02X", (unsigned)(row * 8u));
        const int16_t y = (int16_t)(144 + row * 14u);
        ui_draw_text(canvas, 12, y, label, 1u, UI_COLOR_SECONDARY);
        for (uint8_t column = 0u; column < 8u; ++column) {
            char value[4];
            snprintf(value, sizeof(value), "%02X",
                     (unsigned)snapshot->dsp_registers[row * 8u + column]);
            ui_draw_text(canvas, (int16_t)(42 + column * 48u), y, value, 2u, UI_COLOR_TEXT);
        }
    }
}

void ui_dsp_draw(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid, uint8_t page,
                 ui_target_t pressed_target) {
    if (page == 0u)
        draw_mix_echo(canvas, snapshot, valid);
    else
        draw_registers(canvas, snapshot, valid);
    ui_draw_button(canvas, ui_target_rect(UI_SCREEN_DSP, UI_TARGET_DSP_MIX), "MIX/ECHO", page == 0u,
                   pressed_target == UI_TARGET_DSP_MIX);
    ui_draw_button(canvas, ui_target_rect(UI_SCREEN_DSP, UI_TARGET_DSP_REGISTERS), "REGISTERS",
                   page != 0u, pressed_target == UI_TARGET_DSP_REGISTERS);
}
