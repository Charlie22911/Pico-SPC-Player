#include "display/display_palette.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const display_palette_region_t regions[] = {{14, 388, 256, 12}, {14, 126, 256, 256}};
    assert(display_palette_classify(14, 126, 256, 256, regions, 2) == DISPLAY_PALETTE_SECONDARY);
    assert(display_palette_classify(12, 124, 260, 260, regions, 2) == DISPLAY_PALETTE_MIXED);
    assert(display_palette_classify(280, 124, 170, 256, regions, 2) == DISPLAY_PALETTE_PRIMARY);
    assert(display_palette_classify(14, 382, 256, 6, regions, 2) == DISPLAY_PALETTE_PRIMARY);
    assert(display_palette_classify(14, 388, 256, 12, regions, 2) == DISPLAY_PALETTE_SECONDARY);
    assert(display_palette_classify(0, 0, 450, 600, regions, 2) == DISPLAY_PALETTE_MIXED);
    assert(display_palette_classify(14, 126, 256, 256, NULL, 0) == DISPLAY_PALETTE_PRIMARY);

    uint32_t primary[256];
    uint32_t secondary[256];
    for (uint32_t i = 0; i < 256; ++i) {
        primary[i] = 0x10000000u + i;
        secondary[i] = 0x20000000u + i;
    }
    const uint8_t source[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
    uint32_t output[6];
    const uint32_t want_primary[] = {0x10000012, 0x10000034, 0x10000056,
                                     0x10000078, 0x1000009a, 0x100000bc};
    const uint32_t want_secondary[] = {0x20000012, 0x20000034, 0x20000056,
                                       0x20000078, 0x2000009a, 0x200000bc};
    const uint32_t want_mixed[] = {0x10000012, 0x10000034, 0x20000056,
                                   0x20000078, 0x1000009a, 0x100000bc};
    const display_palette_region_t middle = {4, 3, 4, 2};
    display_palette_expand_row(output, source, 6, 0, 3, DISPLAY_PALETTE_PRIMARY,
                               primary, secondary, &middle, 1);
    assert(memcmp(output, want_primary, sizeof(output)) == 0);
    display_palette_expand_row(output, source, 6, 0, 3, DISPLAY_PALETTE_SECONDARY,
                               primary, secondary, &middle, 1);
    assert(memcmp(output, want_secondary, sizeof(output)) == 0);
    display_palette_expand_row(output, source, 6, 0, 3, DISPLAY_PALETTE_MIXED,
                               primary, secondary, &middle, 1);
    assert(memcmp(output, want_mixed, sizeof(output)) == 0);
    display_palette_expand_row(output, source, 6, 0, 5, DISPLAY_PALETTE_MIXED,
                               primary, secondary, &middle, 1);
    assert(memcmp(output, want_primary, sizeof(output)) == 0);
    puts("Uniform and mixed display palette output OK");
    return 0;
}
