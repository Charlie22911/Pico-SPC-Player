#include "ui_layout.h"

#include <stdbool.h>

const ui_rect_t UI_RECT_VIEW = {338, 0, 100, 100};
const ui_rect_t UI_RECT_HEADER_BUTTON = {338, 8, 100, 84};
const ui_rect_t UI_RECT_VOLUME = {12, 488, 136, 100};
const ui_rect_t UI_RECT_PLAY_PAUSE = {157, 488, 136, 100};
const ui_rect_t UI_RECT_RESTART = {302, 488, 136, 100};

static const ui_rect_t close_rect = {338, 0, 100, 100};
static const ui_rect_t menu_rects[6] = {
    {12, 128, 207, 100},  {231, 128, 207, 100}, {12, 240, 207, 100},
    {231, 240, 207, 100}, {12, 352, 207, 100},  {231, 352, 207, 100},
};
static const ui_rect_t volume_down_rect = {12, 180, 207, 100};
static const ui_rect_t volume_up_rect = {231, 180, 207, 100};
static const ui_rect_t voice_card_rects[4] = {
    {12, 104, 207, 128},
    {231, 104, 207, 128},
    {12, 240, 207, 128},
    {231, 240, 207, 128},
};
static const ui_rect_t subpage_rects[2] = {
    {12, 380, 207, 100},
    {231, 380, 207, 100},
};
static const ui_rect_t library_row_rects[3] = {
    {12, 148, 426, 76},
    {12, 236, 426, 76},
    {12, 324, 426, 76},
};
static const ui_rect_t library_control_rects[3] = {
    {12, 412, 132, 64},
    {153, 412, 132, 64},
    {294, 412, 144, 64},
};
static const ui_rect_t visualizer_rate_rect = {12, 196, 426, 42};

static bool contains(ui_rect_t rect, uint16_t x, uint16_t y) {
    return x >= (uint16_t)rect.x && y >= (uint16_t)rect.y && x < (uint16_t)(rect.x + rect.width) &&
           y < (uint16_t)(rect.y + rect.height);
}

ui_rect_t ui_target_rect(ui_screen_t screen, ui_target_t target) {
    if (target == UI_TARGET_VIEW && screen == UI_SCREEN_PLAYER) {
        return UI_RECT_HEADER_BUTTON;
    }
    if (target == UI_TARGET_CLOSE && screen != UI_SCREEN_PLAYER) {
        return UI_RECT_HEADER_BUTTON;
    }
    if (target == UI_TARGET_VOLUME)
        return UI_RECT_VOLUME;
    if (target == UI_TARGET_PLAY_PAUSE)
        return UI_RECT_PLAY_PAUSE;
    if (target == UI_TARGET_RESTART)
        return UI_RECT_RESTART;
    if (screen == UI_SCREEN_VOLUME && target == UI_TARGET_VOLUME_DOWN) {
        return volume_down_rect;
    }
    if (screen == UI_SCREEN_VOLUME && target == UI_TARGET_VOLUME_UP) {
        return volume_up_rect;
    }
    if (screen == UI_SCREEN_VIEW_MENU && target >= UI_TARGET_ARAM && target <= UI_TARGET_SETTINGS) {
        return menu_rects[target - UI_TARGET_ARAM];
    }
    if (screen == UI_SCREEN_VOICES && target >= UI_TARGET_VOICE_CARD_1 &&
        target <= UI_TARGET_VOICE_CARD_4) {
        return voice_card_rects[target - UI_TARGET_VOICE_CARD_1];
    }
    if (screen == UI_SCREEN_VOICES && target >= UI_TARGET_VOICE_BANK_1_4 &&
        target <= UI_TARGET_VOICE_BANK_5_8) {
        return subpage_rects[target - UI_TARGET_VOICE_BANK_1_4];
    }
    if (screen == UI_SCREEN_DSP && target >= UI_TARGET_DSP_MIX &&
        target <= UI_TARGET_DSP_REGISTERS) {
        return subpage_rects[target - UI_TARGET_DSP_MIX];
    }
    if (screen == UI_SCREEN_LIBRARY && target >= UI_TARGET_LIBRARY_ROW_1 &&
        target <= UI_TARGET_LIBRARY_ROW_3) {
        return library_row_rects[target - UI_TARGET_LIBRARY_ROW_1];
    }
    if (screen == UI_SCREEN_LIBRARY && target >= UI_TARGET_LIBRARY_BACK &&
        target <= UI_TARGET_LIBRARY_NEXT) {
        return library_control_rects[target - UI_TARGET_LIBRARY_BACK];
    }
    if (screen == UI_SCREEN_SETTINGS && target == UI_TARGET_VISUALIZER_RATE)
        return visualizer_rate_rect;
    if (screen == UI_SCREEN_ARAM && target == UI_TARGET_ARAM_MAP)
        return (ui_rect_t){14, 126, 422, 310};
    return (ui_rect_t){0, 0, 0, 0};
}

ui_target_t ui_hit_test(ui_screen_t screen, uint16_t x, uint16_t y) {
    if (screen == UI_SCREEN_PLAYER && contains(UI_RECT_VIEW, x, y)) {
        return UI_TARGET_VIEW;
    }
    if (screen != UI_SCREEN_PLAYER && contains(close_rect, x, y)) {
        return UI_TARGET_CLOSE;
    }
    if (contains(UI_RECT_VOLUME, x, y))
        return UI_TARGET_VOLUME;
    if (contains(UI_RECT_PLAY_PAUSE, x, y))
        return UI_TARGET_PLAY_PAUSE;
    if (contains(UI_RECT_RESTART, x, y))
        return UI_TARGET_RESTART;

    if (screen == UI_SCREEN_VOLUME) {
        if (contains(volume_down_rect, x, y))
            return UI_TARGET_VOLUME_DOWN;
        if (contains(volume_up_rect, x, y))
            return UI_TARGET_VOLUME_UP;
    }
    if (screen == UI_SCREEN_VIEW_MENU) {
        for (uint32_t i = 0; i < 6u; ++i) {
            if (contains(menu_rects[i], x, y)) {
                return (ui_target_t)(UI_TARGET_ARAM + i);
            }
        }
    }
    if (screen == UI_SCREEN_VOICES) {
        for (uint32_t i = 0u; i < 4u; ++i) {
            if (contains(voice_card_rects[i], x, y)) {
                return (ui_target_t)(UI_TARGET_VOICE_CARD_1 + i);
            }
        }
        if (contains(subpage_rects[0], x, y))
            return UI_TARGET_VOICE_BANK_1_4;
        if (contains(subpage_rects[1], x, y))
            return UI_TARGET_VOICE_BANK_5_8;
    }
    if (screen == UI_SCREEN_DSP) {
        if (contains(subpage_rects[0], x, y))
            return UI_TARGET_DSP_MIX;
        if (contains(subpage_rects[1], x, y))
            return UI_TARGET_DSP_REGISTERS;
    }
    if (screen == UI_SCREEN_LIBRARY) {
        for (uint32_t i = 0u; i < 3u; ++i) {
            if (contains(library_row_rects[i], x, y)) {
                return (ui_target_t)(UI_TARGET_LIBRARY_ROW_1 + i);
            }
        }
        for (uint32_t i = 0u; i < 3u; ++i) {
            if (contains(library_control_rects[i], x, y)) {
                return (ui_target_t)(UI_TARGET_LIBRARY_BACK + i);
            }
        }
    }
    if (screen == UI_SCREEN_SETTINGS && contains(visualizer_rate_rect, x, y))
        return UI_TARGET_VISUALIZER_RATE;
    if (screen == UI_SCREEN_ARAM && contains((ui_rect_t){14, 126, 422, 310}, x, y))
        return UI_TARGET_ARAM_MAP;
    return UI_TARGET_NONE;
}

ui_rect_t ui_align_blit_rect(ui_rect_t rect, uint16_t canvas_width, uint16_t canvas_height) {
    if (rect.width <= 0 || rect.height <= 0)
        return (ui_rect_t){0, 0, 0, 0};

    int32_t x0 = rect.x;
    int32_t y0 = rect.y;
    int32_t x1 = x0 + rect.width;
    int32_t y1 = y0 + rect.height;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > canvas_width)
        x1 = canvas_width;
    if (y1 > canvas_height)
        y1 = canvas_height;
    if (x1 <= x0 || y1 <= y0)
        return (ui_rect_t){0, 0, 0, 0};

    x0 &= ~1;
    x1 = (x1 + 1) & ~1;
    if (x1 > canvas_width)
        x1 = canvas_width;
    return (ui_rect_t){
        (int16_t)x0,
        (int16_t)y0,
        (int16_t)(x1 - x0),
        (int16_t)(y1 - y0),
    };
}
