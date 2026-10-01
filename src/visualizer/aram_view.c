#include "aram_view.h"

#include <stddef.h>
#include <string.h>
#if PICO_SPC_UART_PERFORMANCE
#include "pico/time.h"
static uint32_t scale_max_us;
#endif

static uint8_t pair_colors[64], pair_keep[64], pair_fade[256];
static bool activity_tables_ready;

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

static void prepare_activity_tables(void) {
    if (activity_tables_ready) return;
    for (unsigned code = 0u; code < 64u; ++code) {
        const unsigned left = (code & 1u) | ((code >> 1u) & 2u) | ((code >> 2u) & 4u);
        const unsigned right = ((code >> 1u) & 1u) | ((code >> 2u) & 2u) | ((code >> 3u) & 4u);
        pair_colors[code] = (uint8_t)((left ? (unsigned)activity_color((uint8_t)left) << 4u : 0u) |
                                      (right ? (unsigned)activity_color((uint8_t)right) : 0u));
        pair_keep[code] = (uint8_t)((left ? 0u : 0xf0u) | (right ? 0u : 0x0fu));
    }
    for (unsigned old = 0u; old < 256u; ++old)
        pair_fade[old] = (uint8_t)(fade_color((uint8_t)(old >> 4u)) << 4u |
                                  fade_color((uint8_t)(old & 15u)));
    activity_tables_ready = true;
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
    prepare_activity_tables();
    const uint8_t *read = snapshot->read, *write = snapshot->write, *execute = snapshot->execute;
    uint8_t *destination = view->pixels;
    for (uint32_t byte = 0u; byte < ARAM_ACTIVITY_BITMAP_BYTES; ++byte, destination += 4u) {
        unsigned r = read[byte], w = write[byte], e = execute[byte];
        if ((r | w | e) == 0u) {
            uint32_t old;
            memcpy(&old, destination, sizeof(old));
            if (old != 0xaaaaaaaau)
                for (unsigned pair = 0u; pair < 4u; ++pair)
                    destination[pair] = pair_fade[destination[pair]];
            continue;
        }
        for (unsigned pair = 0u; pair < 4u; ++pair) {
            const unsigned code = (r & 3u) | ((w & 3u) << 2u) | ((e & 3u) << 4u);
            destination[pair] = (uint8_t)((pair_fade[destination[pair]] & pair_keep[code]) |
                                          pair_colors[code]);
            r >>= 2u; w >>= 2u; e >>= 2u;
        }
    }
    view->generation = snapshot->generation;
    view->sequence = snapshot->sequence;
    view->request = snapshot->request;
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

#if PICO_SPC_UART_PERFORMANCE
    const uint32_t started_us = time_us_32();
#endif
    typedef struct { uint8_t byte0, byte1, shift0, shift1; } scale_pair_t;
    static scale_pair_t large_pairs[211];
    static uint8_t large_y_map[310];
    static bool large_maps_ready;
    if (!large_maps_ready) {
        for (uint16_t pair = 0u; pair < 211u; ++pair) {
            const unsigned x0 = (unsigned)pair * 2u * ARAM_VIEW_WIDTH / 422u;
            const unsigned x1 = ((unsigned)pair * 2u + 1u) * ARAM_VIEW_WIDTH / 422u;
            large_pairs[pair] = (scale_pair_t){(uint8_t)(x0 / 2u), (uint8_t)(x1 / 2u),
                                              (uint8_t)((x0 & 1u) ? 0u : 4u),
                                              (uint8_t)((x1 & 1u) ? 0u : 4u)};
        }
        for (uint16_t y = 0u; y < 310u; ++y)
            large_y_map[y] = (uint8_t)((uint32_t)y * ARAM_VIEW_HEIGHT / 310u);
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
    uint8_t *row = canvas->pixels + (size_t)y0 * canvas->stride_bytes + (size_t)x0 / 2u;
    const uint16_t stride = canvas->stride_bytes;
    const unsigned pairs = (unsigned)(x1 - x0) / 2u;
    const unsigned dx0 = (unsigned)(x0 - destination.x);
    unsigned previous_sy = UINT32_MAX;
    for (int32_t y = y0; y < y1; ++y) {
        const uint32_t dy = (uint32_t)(y - destination.y);
        const unsigned sy = large
                                ? large_y_map[dy]
                                : (uint16_t)(dy * ARAM_VIEW_HEIGHT / (uint16_t)destination.height);
        if (large && sy == previous_sy) {
            memcpy(row, row - stride, pairs);
        } else {
            const uint8_t *source = view->pixels + sy * ARAM_VIEW_STRIDE;
            if (large) {
                const scale_pair_t *mapping = large_pairs + dx0 / 2u;
                for (unsigned pair = 0u; pair < pairs; ++pair) {
                    const scale_pair_t at = mapping[pair];
                    const unsigned left = (unsigned)source[at.byte0] >> at.shift0;
                    const unsigned right = (unsigned)source[at.byte1] >> at.shift1;
                    row[pair] = (uint8_t)((left << 4u) | (right & 15u));
                }
            } else {
                for (unsigned pair = 0u; pair < pairs; ++pair) {
                    const unsigned dx = dx0 + pair * 2u;
                    const unsigned sx0 = dx * ARAM_VIEW_WIDTH / (uint16_t)destination.width;
                    const unsigned sx1 = (dx + 1u) * ARAM_VIEW_WIDTH / (uint16_t)destination.width;
                    const unsigned left = (unsigned)source[sx0 / 2u] >> ((sx0 & 1u) ? 0u : 4u);
                    const unsigned right = (unsigned)source[sx1 / 2u] >> ((sx1 & 1u) ? 0u : 4u);
                    row[pair] = (uint8_t)((left << 4u) | (right & 15u));
                }
            }
        }
        previous_sy = sy;
        row += stride;
    }
#if PICO_SPC_UART_PERFORMANCE
    const uint32_t elapsed = time_us_32() - started_us;
    if (elapsed > scale_max_us) scale_max_us = elapsed;
#endif
}

uint32_t aram_view_scale_max_us(void) {
#if PICO_SPC_UART_PERFORMANCE
    return scale_max_us;
#else
    return 0u;
#endif
}

void aram_view_reset_scale_max(void) {
#if PICO_SPC_UART_PERFORMANCE
    scale_max_us = 0u;
#endif
}
