#include "visualizer/aram_activity.h"
#include "visualizer/aram_view.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void set_view_pixel(aram_view_t *view, uint16_t x, uint16_t y, uint8_t color) {
    const uint32_t address = (uint32_t)y * ARAM_VIEW_WIDTH + x;
    uint8_t *packed = &view->pixels[address >> 1u];
    if ((address & 1u) == 0u)
        *packed = (uint8_t)((*packed & 0x0fu) | (color << 4u));
    else
        *packed = (uint8_t)((*packed & 0xf0u) | color);
}

static void test_scaled_blit(void) {
    aram_view_t view;
    memset(&view, 0, sizeof(view));
    view.valid = true;
    for (uint16_t y = 0u; y < ARAM_VIEW_HEIGHT; ++y) {
        for (uint16_t x = 0u; x < ARAM_VIEW_WIDTH; ++x)
            set_view_pixel(&view, x, y, (uint8_t)((x + y) & 0x0fu));
    }
    set_view_pixel(&view, 0u, 0u, 1u);
    set_view_pixel(&view, 255u, 0u, 2u);
    set_view_pixel(&view, 0u, 255u, 3u);
    set_view_pixel(&view, 255u, 255u, 4u);
    set_view_pixel(&view, 128u, 128u, 5u);

    static uint8_t pixels[UI_INDEXED4_STRIDE(422u) * 310u];
    memset(pixels, 0, sizeof(pixels));
    ui_canvas_t canvas = {
        .pixels = pixels,
        .width = 422u,
        .height = 310u,
        .stride_bytes = UI_INDEXED4_STRIDE(422u),
    };
    ui_canvas_set_clip(&canvas, (ui_rect_t){0, 0, 422, 310});
    aram_view_blit_scaled(&view, &canvas, (ui_rect_t){0, 0, 422, 310});
    assert(ui_canvas_get_pixel(&canvas, 0u, 0u) == 1u);
    assert(ui_canvas_get_pixel(&canvas, 421u, 0u) == 2u);
    assert(ui_canvas_get_pixel(&canvas, 0u, 309u) == 3u);
    assert(ui_canvas_get_pixel(&canvas, 421u, 309u) == 4u);
    assert(ui_canvas_get_pixel(&canvas, 211u, 155u) == 5u);
    assert(pixels[0] == 0x11u);

    memset(pixels, 0xee, sizeof(pixels));
    ui_canvas_set_clip(&canvas, (ui_rect_t){100, 100, 10, 10});
    aram_view_blit_scaled(&view, &canvas, (ui_rect_t){0, 0, 422, 310});
    assert(ui_canvas_get_pixel(&canvas, 98u, 100u) == 14u);
    assert(ui_canvas_get_pixel(&canvas, 100u, 100u) ==
           aram_view_pixel(&view, (uint16_t)(82u * 256u + 60u)));
    assert(ui_canvas_get_pixel(&canvas, 110u, 100u) == 14u);
}

int main(void) {
    aram_activity_t activity;
    aram_activity_init(&activity, 1u);
    uint8_t *read;
    uint8_t *write;
    uint8_t *execute;
    aram_activity_producer_maps(&activity, &read, &write, &execute);
    aram_activity_mark(read, 0x0000u);
    aram_activity_mark(write, 0x00ffu);
    aram_activity_mark(execute, 0x0100u);
    aram_activity_mark(read, 0xffffu);
    aram_activity_mark(write, 0xffffu);
    assert(aram_activity_publish(&activity));

    aram_activity_snapshot_t snapshot = {0};
    assert(aram_activity_acquire(&activity, &snapshot));
    assert(snapshot.generation == 1u);
    assert(aram_activity_test_bit(snapshot.read, 0x0000u));
    assert(aram_activity_test_bit(snapshot.write, 0x00ffu));
    assert(aram_activity_test_bit(snapshot.execute, 0x0100u));
    assert(aram_activity_test_bit(snapshot.read, 0xffffu));
    assert(aram_activity_test_bit(snapshot.write, 0xffffu));

    uint8_t *next_read;
    uint8_t *next_write;
    uint8_t *next_execute;
    aram_activity_producer_maps(&activity, &next_read, &next_write, &next_execute);
    assert(next_read != snapshot.read);
    assert(next_write != snapshot.write);
    assert(next_execute != snapshot.execute);
    aram_activity_mark(next_write, 0x1234u);
    assert(!aram_activity_publish(&activity));

    aram_view_t view;
    aram_view_init(&view);
    aram_view_apply(&view, &snapshot);
    assert(aram_view_pixel(&view, 0x0000u) == UI_COLOR_READ);
    assert(aram_view_pixel(&view, 0x00ffu) == UI_COLOR_WRITE);
    assert(aram_view_pixel(&view, 0x0100u) == UI_COLOR_EXECUTE);
    assert(aram_view_pixel(&view, 0xffffu) == 11u);
    aram_activity_release(&activity, &snapshot);

    assert(aram_activity_publish(&activity));
    assert(aram_activity_acquire(&activity, &snapshot));
    assert(aram_activity_test_bit(snapshot.write, 0x1234u));
    aram_activity_reset_generation(&activity, 2u);
    aram_activity_producer_maps(&activity, &next_read, &next_write, &next_execute);
    assert(!aram_activity_test_bit(next_read, 0x0000u));
    assert(!aram_activity_test_bit(next_write, 0x1234u));
    aram_activity_mark(next_execute, 0xffffu);
    aram_activity_release(&activity, &snapshot);
    assert(aram_activity_publish(&activity));
    assert(aram_activity_acquire(&activity, &snapshot));
    assert(snapshot.generation == 2u);
    assert(aram_activity_test_bit(snapshot.execute, 0xffffu));
    aram_view_apply(&view, &snapshot);
    assert(view.generation == 2u);
    assert(aram_view_pixel(&view, 0x0000u) == UI_COLOR_INACTIVE);
    assert(aram_view_pixel(&view, 0xffffu) == UI_COLOR_EXECUTE);
    aram_activity_release(&activity, &snapshot);

    test_scaled_blit();

    puts("ARAM ownership and mapping contract OK");
    return 0;
}
