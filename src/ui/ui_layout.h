#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <stdint.h>

typedef struct {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
} ui_rect_t;

typedef enum {
    UI_SCREEN_PLAYER = 0,
    UI_SCREEN_VIEW_MENU,
    UI_SCREEN_ARAM,
    UI_SCREEN_VOICES,
    UI_SCREEN_DSP,
    UI_SCREEN_TRACK,
    UI_SCREEN_LIBRARY,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_VOICE_DETAIL,
    UI_SCREEN_VOLUME,
} ui_screen_t;

typedef enum {
    UI_TARGET_NONE = 0,
    UI_TARGET_VIEW,
    UI_TARGET_CLOSE,
    UI_TARGET_VOLUME,
    UI_TARGET_PLAY_PAUSE,
    UI_TARGET_RESTART,
    UI_TARGET_VOLUME_DOWN,
    UI_TARGET_VOLUME_UP,
    UI_TARGET_ARAM,
    UI_TARGET_VOICES,
    UI_TARGET_DSP,
    UI_TARGET_TRACK,
    UI_TARGET_LIBRARY,
    UI_TARGET_SETTINGS,
    UI_TARGET_VOICE_CARD_1,
    UI_TARGET_VOICE_CARD_2,
    UI_TARGET_VOICE_CARD_3,
    UI_TARGET_VOICE_CARD_4,
    UI_TARGET_VOICE_BANK_1_4,
    UI_TARGET_VOICE_BANK_5_8,
    UI_TARGET_DSP_MIX,
    UI_TARGET_DSP_REGISTERS,
    UI_TARGET_LIBRARY_ROW_1,
    UI_TARGET_LIBRARY_ROW_2,
    UI_TARGET_LIBRARY_ROW_3,
    UI_TARGET_LIBRARY_BACK,
    UI_TARGET_LIBRARY_PREV,
    UI_TARGET_LIBRARY_NEXT,
    UI_TARGET_VISUALIZER_RATE,
    UI_TARGET_ARAM_MAP,
} ui_target_t;

extern const ui_rect_t UI_RECT_VIEW;
extern const ui_rect_t UI_RECT_HEADER_BUTTON;
extern const ui_rect_t UI_RECT_VOLUME;
extern const ui_rect_t UI_RECT_PLAY_PAUSE;
extern const ui_rect_t UI_RECT_RESTART;
extern const ui_rect_t UI_RECT_PLAYER_ARAM_MAP;
extern const ui_rect_t UI_RECT_PLAYER_ARAM_DATA_SCALE;

ui_target_t ui_hit_test(ui_screen_t screen, uint16_t x, uint16_t y);
ui_rect_t ui_target_rect(ui_screen_t screen, ui_target_t target);
ui_rect_t ui_align_blit_rect(ui_rect_t rect, uint16_t canvas_width, uint16_t canvas_height);

#endif
