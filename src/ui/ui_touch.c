#include "ui_touch.h"

#include <stdlib.h>

#define UI_DRAG_CANCEL_PIXELS 16

void ui_touch_init(ui_touch_t *touch) {
    *touch = (ui_touch_t){0};
}

ui_touch_event_t ui_touch_update(ui_touch_t *touch, ui_screen_t screen, uint16_t x, uint16_t y,
                                 bool pressed) {
    if (pressed && !touch->pressed) {
        touch->pressed = true;
        touch->cancelled = false;
        touch->start_x = x;
        touch->start_y = y;
        touch->target = ui_hit_test(screen, x, y);
        if (touch->target != UI_TARGET_NONE) {
            return (ui_touch_event_t){UI_TOUCH_PRESS_CHANGED, touch->target};
        }
        return (ui_touch_event_t){UI_TOUCH_NO_EVENT, UI_TARGET_NONE};
    }

    if (pressed && touch->pressed && !touch->cancelled) {
        const int dx = (int)x - (int)touch->start_x;
        const int dy = (int)y - (int)touch->start_y;
        if (abs(dx) > UI_DRAG_CANCEL_PIXELS || abs(dy) > UI_DRAG_CANCEL_PIXELS) {
            touch->cancelled = true;
            return (ui_touch_event_t){UI_TOUCH_CANCELLED, touch->target};
        }
    }

    if (!pressed && touch->pressed) {
        const ui_target_t target = touch->target;
        const bool activate =
            !touch->cancelled && target != UI_TARGET_NONE && ui_hit_test(screen, x, y) == target;
        const bool was_cancelled = touch->cancelled;
        touch->pressed = false;
        touch->cancelled = false;
        touch->target = UI_TARGET_NONE;
        if (activate) {
            return (ui_touch_event_t){UI_TOUCH_ACTIVATED, target};
        }
        if (target != UI_TARGET_NONE) {
            return (ui_touch_event_t){was_cancelled ? UI_TOUCH_PRESS_CHANGED : UI_TOUCH_CANCELLED,
                                      target};
        }
    }
    return (ui_touch_event_t){UI_TOUCH_NO_EVENT, UI_TARGET_NONE};
}
