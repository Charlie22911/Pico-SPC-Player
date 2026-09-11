#include "aram_view.h"

#include <stddef.h>
#include <string.h>

static uint8_t activity_color(uint8_t activity) {
    static const uint8_t colors[8] = {
        UI_COLOR_INACTIVE, UI_COLOR_READ, UI_COLOR_WRITE, 11u, UI_COLOR_EXECUTE, 12u, 13u, 14u,
    };
    return colors[activity & 7u];
}

static uint8_t fade_color(uint8_t color) {
    if (color == UI_COLOR_INACTIVE)
        return UI_COLOR_INACTIVE;
    return color == 15u ? UI_COLOR_INACTIVE : 15u;
}

void aram_view_init(aram_view_t *view) {
    if (view == NULL)
        return;
    memset(view, 0, sizeof(*view));
    memset(view->pixels, 0xaa, sizeof(view->pixels));
}

void aram_view_apply(aram_view_t *view, const aram_activity_snapshot_t *snapshot) {
    if (view == NULL || snapshot == NULL || !snapshot->held)
        return;
    if (!view->valid || view->generation != snapshot->generation) {
        memset(view->pixels, 0xaa, sizeof(view->pixels));
    }
    for (uint32_t address = 0u; address < ARAM_ACTIVITY_ADDRESS_COUNT; address += 2u) {
        uint8_t packed = 0u;
        for (uint32_t offset = 0u; offset < 2u; ++offset) {
            const uint16_t at = (uint16_t)(address + offset);
            const uint8_t activity =
                (uint8_t)(aram_activity_test_bit(snapshot->read, at) ? 1u : 0u) |
                (uint8_t)(aram_activity_test_bit(snapshot->write, at) ? 2u : 0u) |
                (uint8_t)(aram_activity_test_bit(snapshot->execute, at) ? 4u : 0u);
            const uint8_t old = (offset == 0u) ? (uint8_t)(view->pixels[address >> 1u] >> 4u)
                                               : (uint8_t)(view->pixels[address >> 1u] & 0x0fu);
            const uint8_t color = activity != 0u ? activity_color(activity) : fade_color(old);
            packed |= offset == 0u ? (uint8_t)(color << 4u) : color;
        }
        view->pixels[address >> 1u] = packed;
    }
    view->generation = snapshot->generation;
    view->sequence = snapshot->sequence;
    view->request = 0u;
    view->kind = ARAM_VIEW_ACTIVITY;
    view->valid = true;
}

void aram_view_apply_data(aram_view_t *view, const aram_data_snapshot_t *snapshot) {
    if (view == NULL || snapshot == NULL || !snapshot->held || snapshot->pixels == NULL)
        return;
    memcpy(view->pixels, snapshot->pixels, sizeof(view->pixels));
    view->generation = snapshot->generation;
    view->sequence = snapshot->sequence;
    view->request = snapshot->request;
    view->kind = ARAM_VIEW_DATA;
    view->valid = true;
}

void aram_view_invalidate(aram_view_t *view) {
    if (view != NULL)
        view->valid = false;
}

uint8_t aram_view_pixel(const aram_view_t *view, uint16_t address) {
    if (view == NULL)
        return UI_COLOR_INACTIVE;
    const uint8_t packed = view->pixels[address >> 1u];
    return (address & 1u) == 0u ? (uint8_t)(packed >> 4u) : (uint8_t)(packed & 0x0fu);
}

void aram_view_blit(const aram_view_t *view, ui_canvas_t *canvas, int16_t x, int16_t y) {
    if (view == NULL || canvas == NULL || canvas->pixels == NULL || !view->valid ||
        canvas->clip.width < 0 || canvas->clip.height < 0 || x < 0 || y < 0 || (x & 1) != 0) {
        return;
    }
    int32_t x0 = x;
    int32_t y0 = y;
    int32_t x1 = x + (int32_t)ARAM_VIEW_WIDTH;
    int32_t y1 = y + (int32_t)ARAM_VIEW_HEIGHT;
    if (canvas->clip.width > 0 && canvas->clip.height > 0) {
        if (x0 < canvas->clip.x)
            x0 = canvas->clip.x;
        if (y0 < canvas->clip.y)
            y0 = canvas->clip.y;
        const int32_t clip_x1 = canvas->clip.x + canvas->clip.width;
        const int32_t clip_y1 = canvas->clip.y + canvas->clip.height;
        if (x1 > clip_x1)
            x1 = clip_x1;
        if (y1 > clip_y1)
            y1 = clip_y1;
    }
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > canvas->width)
        x1 = canvas->width;
    if (y1 > canvas->height)
        y1 = canvas->height;
    x0 = (x0 + 1) & ~1;
    x1 &= ~1;
    if (x1 <= x0 || y1 <= y0)
        return;

    const size_t bytes = (size_t)(x1 - x0) / 2u;
    const size_t source_x = (size_t)(x0 - x) / 2u;
    for (int32_t row = y0; row < y1; ++row) {
        memcpy(canvas->pixels + (size_t)row * canvas->stride_bytes + (size_t)x0 / 2u,
               view->pixels + (size_t)(row - y) * ARAM_VIEW_STRIDE + source_x, bytes);
    }
}

void aram_view_blit_scaled(const aram_view_t *view, ui_canvas_t *canvas, ui_rect_t destination) {
    if (view == NULL || canvas == NULL || canvas->pixels == NULL || !view->valid ||
        canvas->clip.width < 0 || canvas->clip.height < 0 || destination.width <= 0 ||
        destination.height <= 0 || (destination.x & 1) != 0 || (destination.width & 1) != 0)
        return;

    static uint16_t large_x_map[422];
    static uint16_t large_y_map[310];
    static bool large_maps_ready;
    if (!large_maps_ready) {
        for (uint16_t x = 0u; x < 422u; ++x)
            large_x_map[x] = (uint16_t)((uint32_t)x * ARAM_VIEW_WIDTH / 422u);
        for (uint16_t y = 0u; y < 310u; ++y)
            large_y_map[y] = (uint16_t)((uint32_t)y * ARAM_VIEW_HEIGHT / 310u);
        large_maps_ready = true;
    }

    int32_t x0 = destination.x;
    int32_t y0 = destination.y;
    int32_t x1 = (int32_t)destination.x + destination.width;
    int32_t y1 = (int32_t)destination.y + destination.height;
    if (canvas->clip.width > 0 && canvas->clip.height > 0) {
        if (x0 < canvas->clip.x)
            x0 = canvas->clip.x;
        if (y0 < canvas->clip.y)
            y0 = canvas->clip.y;
        const int32_t clip_x1 = (int32_t)canvas->clip.x + canvas->clip.width;
        const int32_t clip_y1 = (int32_t)canvas->clip.y + canvas->clip.height;
        if (x1 > clip_x1)
            x1 = clip_x1;
        if (y1 > clip_y1)
            y1 = clip_y1;
    }
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > canvas->width)
        x1 = canvas->width;
    if (y1 > canvas->height)
        y1 = canvas->height;
    x0 = (x0 + 1) & ~1;
    x1 &= ~1;
    if (x1 <= x0 || y1 <= y0)
        return;

    const bool large = destination.width == 422 && destination.height == 310;
    for (int32_t y = y0; y < y1; ++y) {
        const uint32_t dy = (uint32_t)(y - destination.y);
        const uint16_t sy = large
                                ? large_y_map[dy]
                                : (uint16_t)(dy * ARAM_VIEW_HEIGHT / (uint16_t)destination.height);
        for (int32_t x = x0; x < x1; x += 2) {
            const uint32_t dx = (uint32_t)(x - destination.x);
            const uint16_t sx0 =
                large ? large_x_map[dx]
                      : (uint16_t)(dx * ARAM_VIEW_WIDTH / (uint16_t)destination.width);
            const uint16_t sx1 =
                large ? large_x_map[dx + 1u]
                      : (uint16_t)((dx + 1u) * ARAM_VIEW_WIDTH / (uint16_t)destination.width);
            const uint8_t left =
                aram_view_pixel(view, (uint16_t)((uint32_t)sy * ARAM_VIEW_WIDTH + sx0));
            const uint8_t right =
                aram_view_pixel(view, (uint16_t)((uint32_t)sy * ARAM_VIEW_WIDTH + sx1));
            canvas->pixels[(size_t)y * canvas->stride_bytes + (size_t)x / 2u] =
                (uint8_t)(left << 4u | right);
        }
    }
}
