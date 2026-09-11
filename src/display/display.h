#ifndef DISPLAY_DISPLAY_H
#define DISPLAY_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DISPLAY_TRANSFER_IDLE = 0,
    DISPLAY_TRANSFER_ACTIVE,
    DISPLAY_TRANSFER_COMPLETE,
    DISPLAY_TRANSFER_ERROR,
} display_transfer_status_t;

typedef struct {
    uint32_t completed_transfers;
    uint32_t completed_aram_maps;
    uint32_t last_transfer_us;
    uint32_t max_transfer_us;
    uint16_t measured_aram_hz_x10;
} display_stats_t;

#define DISPLAY_PALETTE_REGION_CAPACITY 2u

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} display_palette_region_t;

/* All calls are owned by Core 1. Palette entries are host-order RGB565 and are
 * copied into lookup tables. Indexed pixels place the left/even pixel in the
 * high nibble. Rectangles must begin and end on pixel pairs. begin_indexed4()
 * borrows pixels until transfer completion and copies the palette-region list. */
void display_set_palette(const uint16_t palette[16]);
void display_set_secondary_palette(const uint16_t palette[16]);
bool display_begin_indexed4(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                            const uint8_t *pixels, uint16_t stride_bytes,
                            const display_palette_region_t *secondary_regions,
                            uint8_t secondary_region_count);
display_transfer_status_t display_poll(void);
bool display_transfer_active(void);
void display_mark_current_transfer_aram(void);
display_stats_t display_get_stats(void);
void display_cancel(void);

#endif
