#ifndef UI_TOUCH_H
#define UI_TOUCH_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_layout.h"

typedef enum {
    UI_TOUCH_NO_EVENT = 0,
    UI_TOUCH_PRESS_CHANGED,
    UI_TOUCH_ACTIVATED,
    UI_TOUCH_CANCELLED,
} ui_touch_event_type_t;

typedef struct {
    bool pressed;
    bool cancelled;
    uint16_t start_x;
    uint16_t start_y;
    ui_target_t target;
} ui_touch_t;

typedef struct {
    ui_touch_event_type_t type;
    ui_target_t target;
} ui_touch_event_t;

void ui_touch_init(ui_touch_t *touch);
ui_touch_event_t ui_touch_update(ui_touch_t *touch, ui_screen_t screen, uint16_t x, uint16_t y,
                                 bool pressed);

#endif
