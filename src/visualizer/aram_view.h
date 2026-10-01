#ifndef ARAM_VIEW_H
#define ARAM_VIEW_H

#include <stdbool.h>
#include <stdint.h>

#include "aram_activity.h"
#include "aram_data.h"
#include "ui/ui_draw.h"

#define ARAM_VIEW_WIDTH 256u
#define ARAM_VIEW_HEIGHT 256u
#define ARAM_VIEW_STRIDE (ARAM_VIEW_WIDTH / 2u)

typedef enum {
    ARAM_VIEW_ACTIVITY = 0,
    ARAM_VIEW_DATA,
} aram_view_kind_t;

typedef struct {
    uint8_t pixels[ARAM_VIEW_STRIDE * ARAM_VIEW_HEIGHT];
    uint32_t generation;
    uint32_t sequence;
    uint32_t request;
    aram_view_kind_t kind;
    bool valid;
} aram_view_t;

void aram_view_init(aram_view_t *view);
void aram_view_apply(aram_view_t *view, const aram_activity_snapshot_t *snapshot);
void aram_view_apply_data(aram_view_t *view, const aram_data_snapshot_t *snapshot);
void aram_view_invalidate(aram_view_t *view);
uint8_t aram_view_pixel(const aram_view_t *view, uint16_t address);
void aram_view_blit(const aram_view_t *view, ui_canvas_t *canvas, int16_t x, int16_t y);
void aram_view_blit_scaled(const aram_view_t *view, ui_canvas_t *canvas, ui_rect_t destination);
/* Core 1 owns this maximum; reset after an accepted UART reporting window. */
uint32_t aram_view_scale_max_us(void);
void aram_view_reset_scale_max(void);

#endif
