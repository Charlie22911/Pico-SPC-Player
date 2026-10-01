#include "visualizer/aram_view.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void reference_aram_view_apply(aram_view_t *, const aram_activity_snapshot_t *);
void reference_aram_view_blit_scaled(const aram_view_t *, ui_canvas_t *, ui_rect_t);

static aram_view_t actual, expected;
static uint8_t reads[8192], writes[8192], executes[8192];
static uint32_t random_state = 0x12345678u;
static uint32_t random_word(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void compare_activity(uint32_t generation) {
    const aram_activity_snapshot_t snapshot = {
        .read = reads, .write = writes, .execute = executes,
        .generation = generation, .sequence = 19u, .held = true,
    };
    aram_view_apply(&actual, &snapshot);
    reference_aram_view_apply(&expected, &snapshot);
    assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
}

static void test_activity(void) {
    aram_view_init(&actual);
    actual.valid = true;
    actual.generation = 7u;
    for (uint32_t pair = 0; pair < 32768u; ++pair) {
        const uint32_t code = (pair >> 8u) & 63u;
        const uint32_t shift = (pair & 3u) * 2u;
        reads[pair / 4u] |= (uint8_t)((code & 3u) << shift);
        writes[pair / 4u] |= (uint8_t)(((code >> 2u) & 3u) << shift);
        executes[pair / 4u] |= (uint8_t)(((code >> 4u) & 3u) << shift);
        actual.pixels[pair] = (uint8_t)pair;
    }
    expected = actual;
    compare_activity(7u); /* Every activity pair and every old packed byte. */
    for (unsigned pattern = 0; pattern < 5u; ++pattern) {
        for (unsigned i = 0; i < 8192u; ++i) {
            reads[i] = (uint8_t)(pattern == 0u ? 0u : pattern == 1u ? 255u :
                       pattern == 2u ? 0xaau : pattern == 3u ? (uint8_t)(i % 97u == 0u) : (uint8_t)random_word());
            writes[i] = (uint8_t)(pattern == 0u ? 0u : pattern == 1u ? 255u :
                        pattern == 2u ? 0x55u : pattern == 3u ? (uint8_t)(i % 131u == 0u) : (uint8_t)random_word());
            executes[i] = pattern < 2u ? reads[i] : pattern == 2u ? 0xccu :
                          pattern == 3u ? 0u : (uint8_t)random_word();
        }
        compare_activity(7u);
    }
    memset(reads, 0, sizeof(reads));
    memset(writes, 0, sizeof(writes));
    memset(executes, 0, sizeof(executes));
    compare_activity(7u);
    compare_activity(7u);
    compare_activity(8u);
    aram_view_invalidate(&actual);
    expected.valid = false;
    reads[0] = 1u;
    compare_activity(8u);
}

#define CANVAS_BYTES (225u * 480u)
static uint8_t actual_canvas[CANVAS_BYTES + 64u], expected_canvas[CANVAS_BYTES + 64u];
static void compare_scale(ui_rect_t destination, ui_rect_t clip) {
    memset(actual_canvas, 0xe5, sizeof(actual_canvas));
    memcpy(expected_canvas, actual_canvas, sizeof(actual_canvas));
    ui_canvas_t a = {.pixels = actual_canvas + 32u, .width = 450u, .height = 480u,
                     .stride_bytes = 225u, .clip = clip};
    ui_canvas_t e = a;
    e.pixels = expected_canvas + 32u;
    aram_view_blit_scaled(&actual, &a, destination);
    reference_aram_view_blit_scaled(&actual, &e, destination);
    assert(memcmp(actual_canvas, expected_canvas, sizeof(actual_canvas)) == 0);
}

static void test_scaling(void) {
    actual.valid = true;
    for (unsigned i = 0; i < sizeof(actual.pixels); ++i)
        actual.pixels[i] = (uint8_t)(random_word() >> 24u);
    const ui_rect_t large = {14, 120, 422, 310};
    compare_scale(large, (ui_rect_t){0});
    for (int16_t y = 120; y < 430; ++y)
        compare_scale(large, (ui_rect_t){31, y, 391, 3});
    compare_scale(large, (ui_rect_t){0, 0, -1, -1});
    compare_scale((ui_rect_t){-12, -10, 422, 310}, (ui_rect_t){0});
    compare_scale((ui_rect_t){100, 100, 1, 40}, (ui_rect_t){0});
    for (unsigned i = 0; i < 200u; ++i) {
        const ui_rect_t destination = {
            (int16_t)((int32_t)(random_word() % 100u) * 2 - 40),
            (int16_t)((int32_t)(random_word() % 100u) - 30),
            (int16_t)(2u + 2u * (random_word() % 250u)),
            (int16_t)(1u + random_word() % 580u),
        };
        const ui_rect_t clip = {(int16_t)(random_word() % 450u), (int16_t)(random_word() % 480u),
                               (int16_t)(1u + random_word() % 450u), (int16_t)(1u + random_word() % 480u)};
        compare_scale(destination, clip);
    }
}

int main(void) {
    test_activity();
    test_scaling();
    puts("ARAM Activity and scaler match the original implementation byte-for-byte");
    return 0;
}
