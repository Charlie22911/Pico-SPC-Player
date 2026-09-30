#ifndef DISPLAY_PALETTE_H
#define DISPLAY_PALETTE_H

#include "display.h"

typedef enum {
    DISPLAY_PALETTE_PRIMARY,
    DISPLAY_PALETTE_SECONDARY,
    DISPLAY_PALETTE_MIXED,
} display_palette_mode_t;

/* Regions and transfers are pair-aligned by display_begin_indexed4(). A union
 * of partial regions may conservatively use the mixed path. */
static inline display_palette_mode_t display_palette_classify(
    uint16_t x, uint16_t y, uint16_t width, uint16_t height,
    const display_palette_region_t *regions, uint8_t count) {
    const uint32_t right = (uint32_t)x + width;
    const uint32_t bottom = (uint32_t)y + height;
    bool intersects = false;
    for (uint8_t i = 0u; i < count; ++i) {
        const display_palette_region_t *region = &regions[i];
        const uint32_t region_right = (uint32_t)region->x + region->width;
        const uint32_t region_bottom = (uint32_t)region->y + region->height;
        if (x >= region->x && y >= region->y && right <= region_right && bottom <= region_bottom)
            return DISPLAY_PALETTE_SECONDARY;
        if (x < region_right && region->x < right && y < region_bottom && region->y < bottom)
            intersects = true;
    }
    return intersects ? DISPLAY_PALETTE_MIXED : DISPLAY_PALETTE_PRIMARY;
}

static inline void display_palette_expand_row(
    uint32_t *destination, const uint8_t *source, uint16_t pairs, uint16_t x, uint16_t y,
    display_palette_mode_t mode, const uint32_t primary[256], const uint32_t secondary[256],
    const display_palette_region_t *regions, uint8_t count) {
    if (mode != DISPLAY_PALETTE_MIXED) {
        const uint32_t *lut = mode == DISPLAY_PALETTE_SECONDARY ? secondary : primary;
        for (uint16_t pair = 0u; pair < pairs; ++pair)
            destination[pair] = lut[source[pair]];
        return;
    }
    for (uint16_t pair = 0u; pair < pairs; ++pair) {
        const uint16_t pair_x = (uint16_t)(x + pair * 2u);
        const uint32_t *lut = primary;
        for (uint8_t i = 0u; i < count; ++i) {
            const display_palette_region_t *region = &regions[i];
            if (pair_x >= region->x && pair_x < (uint32_t)region->x + region->width &&
                y >= region->y && y < (uint32_t)region->y + region->height) {
                lut = secondary;
                break;
            }
        }
        destination[pair] = lut[source[pair]];
    }
}

#endif
