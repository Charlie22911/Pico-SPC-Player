#include "ui_voices.h"

#include <stdio.h>

static const char *envelope_mode_name(uint8_t mode) {
    static const char *names[4] = {"RELEASE", "ATTACK", "DECAY", "SUSTAIN"};
    return mode < 4u ? names[mode] : "UNKNOWN";
}

void ui_voices_draw_summary(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid) {
    for (uint8_t voice = 0u; voice < 8u; ++voice) {
        const int16_t y = (int16_t)(130 + voice * 31u);
        char label[4];
        snprintf(label, sizeof(label), "%u", (unsigned)voice + 1u);
        ui_draw_text(canvas, 286, y, label, 2u, UI_COLOR_SECONDARY);
        const ui_rect_t bar = {306, (int16_t)(y - 2), 130, 18};
        ui_draw_fill(canvas, bar, UI_COLOR_INACTIVE);
        if (valid) {
            const spc_voice_snapshot_t *state = &snapshot->voices[voice];
            const uint16_t width = (uint16_t)((uint32_t)state->envelope * 126u / 0x7ffu);
            ui_draw_fill(canvas, (ui_rect_t){308, y, (int16_t)width, 14}, UI_COLOR_WRITE);
            const int16_t pitch_x = (int16_t)(308u + (uint32_t)state->pitch * 126u / 0x3fffu);
            ui_draw_fill(canvas, (ui_rect_t){pitch_x, y, 2, 14}, UI_COLOR_EXECUTE);
        }
        ui_draw_outline(canvas, bar, UI_COLOR_BORDER, 2u);
    }
}

static void draw_voice_card(ui_canvas_t *canvas, ui_rect_t rect, const spc_voice_snapshot_t *voice,
                            uint8_t number, bool pressed) {
    ui_draw_fill(canvas, rect, pressed ? UI_COLOR_ACCENT : UI_COLOR_PANEL);
    ui_draw_outline(canvas, rect, UI_COLOR_BORDER, 3u);
    const uint8_t text_color = pressed ? UI_COLOR_BACKGROUND : UI_COLOR_TEXT;
    char line[32];
    snprintf(line, sizeof(line), "VOICE %u", (unsigned)number + 1u);
    ui_draw_text(canvas, (int16_t)(rect.x + 10), (int16_t)(rect.y + 8), line, 2u, text_color);
    snprintf(line, sizeof(line), "SRC %02X  P %04X", voice->source_number, voice->pitch);
    ui_draw_text(canvas, (int16_t)(rect.x + 10), (int16_t)(rect.y + 36), line, 2u,
                 pressed ? UI_COLOR_BACKGROUND : UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "ENV %03X  OUT %+d", voice->envelope, (int)voice->outx);
    ui_draw_text_clipped(
        canvas,
        (ui_rect_t){(int16_t)(rect.x + 10), (int16_t)(rect.y + 62), (int16_t)(rect.width - 20), 16},
        line, 2u, pressed ? UI_COLOR_BACKGROUND : UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "VL %+d  VR %+d", (int)voice->volume_left,
             (int)voice->volume_right);
    ui_draw_text_clipped(
        canvas,
        (ui_rect_t){(int16_t)(rect.x + 10), (int16_t)(rect.y + 86), (int16_t)(rect.width - 20), 16},
        line, 2u, pressed ? UI_COLOR_BACKGROUND : UI_COLOR_SECONDARY);
    const uint16_t width =
        (uint16_t)((uint32_t)voice->envelope * (uint16_t)(rect.width - 20) / 0x7ffu);
    ui_draw_fill(canvas,
                 (ui_rect_t){(int16_t)(rect.x + 10), (int16_t)(rect.y + 108),
                             (int16_t)(rect.width - 20), 10},
                 UI_COLOR_INACTIVE);
    ui_draw_fill(canvas,
                 (ui_rect_t){(int16_t)(rect.x + 10), (int16_t)(rect.y + 108), (int16_t)width, 10},
                 pressed ? UI_COLOR_BACKGROUND : UI_COLOR_WRITE);
}

void ui_voices_draw_bank(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid,
                         uint8_t bank, ui_target_t pressed_target) {
    if (!valid) {
        ui_draw_text(canvas, 72, 276, "VOICE DATA UNAVAILABLE", 2u, UI_COLOR_SECONDARY);
    } else {
        for (uint8_t card = 0u; card < 4u; ++card) {
            const ui_target_t target = (ui_target_t)(UI_TARGET_VOICE_CARD_1 + card);
            const uint8_t voice = (uint8_t)(bank * 4u + card);
            draw_voice_card(canvas, ui_target_rect(UI_SCREEN_VOICES, target),
                            &snapshot->voices[voice], voice, pressed_target == target);
        }
    }
    ui_draw_button(canvas, ui_target_rect(UI_SCREEN_VOICES, UI_TARGET_VOICE_BANK_1_4), "1-4",
                   bank == 0u, pressed_target == UI_TARGET_VOICE_BANK_1_4);
    ui_draw_button(canvas, ui_target_rect(UI_SCREEN_VOICES, UI_TARGET_VOICE_BANK_5_8), "5-8",
                   bank != 0u, pressed_target == UI_TARGET_VOICE_BANK_5_8);
}

void ui_voices_draw_detail(ui_canvas_t *canvas, const spc_snapshot_t *snapshot, bool valid,
                           uint8_t voice_number) {
    char line[48];
    snprintf(line, sizeof(line), "VOICE %u DETAIL", (unsigned)voice_number + 1u);
    ui_draw_text(canvas, 12, 108, line, 3u, UI_COLOR_TEXT);
    if (!valid || voice_number >= 8u) {
        ui_draw_text(canvas, 72, 276, "VOICE DATA UNAVAILABLE", 2u, UI_COLOR_SECONDARY);
        return;
    }
    const spc_voice_snapshot_t *voice = &snapshot->voices[voice_number];
    snprintf(line, sizeof(line), "SRCN %02X    PITCH %04X", voice->source_number, voice->pitch);
    ui_draw_text(canvas, 12, 164, line, 2u, UI_COLOR_ACCENT);
    snprintf(line, sizeof(line), "VOL L %+d    VOL R %+d", (int)voice->volume_left,
             (int)voice->volume_right);
    ui_draw_text(canvas, 12, 198, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "ENV %03X    MODE %s", voice->envelope,
             envelope_mode_name(voice->envelope_mode));
    ui_draw_text(canvas, 12, 232, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "BRR $%04X    OUTX %+d", voice->brr_address, (int)voice->outx);
    ui_draw_text(canvas, 12, 266, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "ADSR %02X %02X    GAIN %02X", voice->adsr0, voice->adsr1,
             voice->gain);
    ui_draw_text(canvas, 12, 300, line, 2u, UI_COLOR_SECONDARY);
    snprintf(line, sizeof(line), "KONBIT %u  KOFFBIT %u  ENDX %u", voice->key_on, voice->key_off,
             voice->endx);
    ui_draw_text(canvas, 12, 334, line, 2u, UI_COLOR_SECONDARY);
    ui_draw_text(canvas, 12, 382, "READ-ONLY DSP STATE", 2u, UI_COLOR_ACCENT);
}
