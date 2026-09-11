#include "player/player_commands.h"
#include "ui/ui.h"
#include "ui/ui_font.h"
#include "ui/ui_layout.h"
#include "ui/ui_marquee.h"
#include "ui/ui_touch.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_touch_targets_and_cancellation(void) {
    const ui_rect_t header_button = ui_target_rect(UI_SCREEN_PLAYER, UI_TARGET_VIEW);
    assert(header_button.x == 338);
    assert(header_button.y == 8);
    assert(header_button.width == 100);
    assert(header_button.height == 84);
    assert(ui_hit_test(UI_SCREEN_PLAYER, 12, 488) == UI_TARGET_VOLUME);
    assert(ui_hit_test(UI_SCREEN_PLAYER, 147, 587) == UI_TARGET_VOLUME);
    assert(ui_hit_test(UI_SCREEN_PLAYER, 148, 500) == UI_TARGET_NONE);
    assert(ui_hit_test(UI_SCREEN_PLAYER, 157, 488) == UI_TARGET_PLAY_PAUSE);
    assert(ui_hit_test(UI_SCREEN_PLAYER, 302, 488) == UI_TARGET_RESTART);
    assert(ui_hit_test(UI_SCREEN_PLAYER, 338, 0) == UI_TARGET_VIEW);
    assert(ui_hit_test(UI_SCREEN_PLAYER, 437, 99) == UI_TARGET_VIEW);
    assert(ui_hit_test(UI_SCREEN_ARAM, 338, 0) == UI_TARGET_CLOSE);
    assert(ui_hit_test(UI_SCREEN_ARAM, 449, 599) == UI_TARGET_NONE);

    const ui_rect_t aram_tile = ui_target_rect(UI_SCREEN_VIEW_MENU, UI_TARGET_ARAM);
    assert(aram_tile.y == 128);
    assert(ui_hit_test(UI_SCREEN_VIEW_MENU, 12, 127) == UI_TARGET_NONE);
    assert(ui_hit_test(UI_SCREEN_VIEW_MENU, 12, 128) == UI_TARGET_ARAM);
    assert(ui_hit_test(UI_SCREEN_VOICES, 12, 104) == UI_TARGET_VOICE_CARD_1);
    assert(ui_hit_test(UI_SCREEN_VOICES, 231, 240) == UI_TARGET_VOICE_CARD_4);
    assert(ui_hit_test(UI_SCREEN_VOICES, 12, 380) == UI_TARGET_VOICE_BANK_1_4);
    assert(ui_hit_test(UI_SCREEN_DSP, 231, 380) == UI_TARGET_DSP_REGISTERS);

    const ui_rect_t aligned_left = ui_align_blit_rect((ui_rect_t){12, 128, 207, 100}, 450, 600);
    assert(aligned_left.x == 12);
    assert(aligned_left.width == 208);
    const ui_rect_t aligned_right = ui_align_blit_rect((ui_rect_t){231, 128, 207, 100}, 450, 600);
    assert(aligned_right.x == 230);
    assert(aligned_right.width == 208);

    ui_touch_t touch;
    ui_touch_init(&touch);
    ui_touch_event_t event = ui_touch_update(&touch, UI_SCREEN_PLAYER, 200, 520, true);
    assert(event.type == UI_TOUCH_PRESS_CHANGED);
    assert(event.target == UI_TARGET_PLAY_PAUSE);
    event = ui_touch_update(&touch, UI_SCREEN_PLAYER, 220, 520, true);
    assert(event.type == UI_TOUCH_CANCELLED);
    event = ui_touch_update(&touch, UI_SCREEN_PLAYER, 220, 520, false);
    assert(event.type == UI_TOUCH_PRESS_CHANGED);
    assert(event.target == UI_TARGET_PLAY_PAUSE);

    ui_touch_init(&touch);
    (void)ui_touch_update(&touch, UI_SCREEN_PLAYER, 200, 520, true);
    event = ui_touch_update(&touch, UI_SCREEN_PLAYER, 200, 520, false);
    assert(event.type == UI_TOUCH_ACTIVATED);
    assert(event.target == UI_TARGET_PLAY_PAUSE);
}

static void tap(ui_t *ui, player_command_queue_t *commands, uint16_t x, uint16_t y) {
    (void)ui_handle_touch(ui, x, y, true, commands);
    (void)ui_handle_touch(ui, x, y, false, commands);
}

static void test_screen_routing_and_visual_rate(void) {
    player_command_queue_t commands;
    player_command_queue_init(&commands);
    const ui_model_t model = {.visualizer_hz = 30u};
    ui_t ui;
    ui_init(&ui, &model);
    assert(ui.screen == UI_SCREEN_PLAYER);

    tap(&ui, &commands, 380u, 50u);
    assert(ui.screen == UI_SCREEN_VIEW_MENU);
    tap(&ui, &commands, 100u, 170u);
    assert(ui.screen == UI_SCREEN_ARAM);
    tap(&ui, &commands, 380u, 50u);
    assert(ui.screen == UI_SCREEN_PLAYER);

    tap(&ui, &commands, 380u, 50u);
    tap(&ui, &commands, 300u, 390u);
    assert(ui.screen == UI_SCREEN_SETTINGS);
    tap(&ui, &commands, 100u, 216u);
    uint8_t requested_hz = 0u;
    assert(ui_take_visual_rate_request(&ui, &requested_hz));
    assert(requested_hz == 60u);
    tap(&ui, &commands, 100u, 216u);
    assert(ui_take_visual_rate_request(&ui, &requested_hz));
    assert(requested_hz == 30u);
}

static void test_command_queue_preserves_order_and_reports_full(void) {
    player_command_queue_t queue;
    player_command_queue_init(&queue);
    for (uint8_t i = 0; i < PLAYER_COMMAND_CAPACITY; ++i) {
        const player_command_t in = {
            i == 3 ? PLAYER_COMMAND_RESTART : PLAYER_COMMAND_SET_VOLUME,
            i,
        };
        assert(player_command_push(&queue, in));
    }
    assert(!player_command_push(&queue, (player_command_t){PLAYER_COMMAND_SET_PAUSED, 1}));

    for (uint8_t i = 0; i < PLAYER_COMMAND_CAPACITY; ++i) {
        player_command_t out = {0};
        assert(player_command_pop(&queue, &out));
        assert(out.type == (i == 3 ? PLAYER_COMMAND_RESTART : PLAYER_COMMAND_SET_VOLUME));
        assert(out.value == i);
    }
    player_command_t out = {0};
    assert(!player_command_pop(&queue, &out));
}

static void test_dirty_render_matches_full_render(void) {
    static const uint8_t open_parenthesis[7] = {2, 4, 8, 8, 8, 4, 2};
    static const uint8_t close_parenthesis[7] = {8, 4, 2, 2, 2, 4, 8};
    static const uint8_t underscore[7] = {0, 0, 0, 0, 0, 0, 31};
    assert(memcmp(ui_font_glyph('('), open_parenthesis, sizeof(open_parenthesis)) == 0);
    assert(memcmp(ui_font_glyph(')'), close_parenthesis, sizeof(close_parenthesis)) == 0);
    assert(memcmp(ui_font_glyph('_'), underscore, sizeof(underscore)) == 0);

    static uint8_t partial_pixels[UI_INDEXED4_STRIDE(450u) * 600u];
    static uint8_t full_pixels[UI_INDEXED4_STRIDE(450u) * 600u];
    ui_canvas_t partial = {
        .pixels = partial_pixels,
        .width = 450u,
        .height = 600u,
        .stride_bytes = UI_INDEXED4_STRIDE(450u),
    };
    ui_canvas_t full = {
        .pixels = full_pixels,
        .width = 450u,
        .height = 600u,
        .stride_bytes = UI_INDEXED4_STRIDE(450u),
    };
    const ui_model_t initial = {
        .player_ready = true,
        .audio_ready = true,
        .volume_step = 8u,
    };
    ui_t partial_ui;
    ui_t full_ui;
    ui_init(&partial_ui, &initial);
    ui_init(&full_ui, &initial);
    ui_render(&partial_ui, &partial);
    ui_render(&full_ui, &full);

    ui_model_t updated = initial;
    updated.track_frames = 65u * 32000u;
    updated.peak_left = 32768u;
    updated.peak_right = 16384u;
    assert(ui_set_model(&partial_ui, &updated));
    assert(ui_set_model(&full_ui, &updated));
    ui_render_dirty(&partial_ui, &partial, (ui_dirty_t){true, false, {0, 408, 450, 64}});
    ui_render(&full_ui, &full);
    assert(memcmp(partial_pixels, full_pixels, sizeof(partial_pixels)) == 0);

    partial_ui.screen = UI_SCREEN_TRACK;
    full_ui.screen = UI_SCREEN_TRACK;
    ui_render(&partial_ui, &partial);
    ui_render(&full_ui, &full);
    updated.track_frames += 32000u;
    assert(ui_set_model(&partial_ui, &updated));
    assert(ui_set_model(&full_ui, &updated));
    ui_render_dirty(&partial_ui, &partial, (ui_dirty_t){true, false, {0, 260, 450, 40}});
    ui_render(&full_ui, &full);
    assert(memcmp(partial_pixels, full_pixels, sizeof(partial_pixels)) == 0);

    partial_ui.screen = UI_SCREEN_VIEW_MENU;
    full_ui.screen = UI_SCREEN_VIEW_MENU;
    ui_render(&partial_ui, &partial);
    ui_render(&full_ui, &full);
    partial_ui.pressed_target = UI_TARGET_ARAM;
    full_ui.pressed_target = UI_TARGET_ARAM;
    ui_render_dirty(
        &partial_ui, &partial,
        (ui_dirty_t){
            true, false,
            ui_align_blit_rect(ui_target_rect(UI_SCREEN_VIEW_MENU, UI_TARGET_ARAM), 450u, 600u)});
    ui_render(&full_ui, &full);
    assert(memcmp(partial_pixels, full_pixels, sizeof(partial_pixels)) == 0);
}

static void test_marquee_and_stale_titles(void) {
    assert(ui_marquee_offset(80u, 100u, 5000u) == 0);
    assert(ui_marquee_offset(150u, 100u, 799u) == 0);
    assert(ui_marquee_offset(150u, 100u, 800u) == 0);
    assert(ui_marquee_offset(150u, 100u, 1800u) == 25);
    assert(ui_marquee_offset(150u, 100u, 2800u) == 50);
    assert(ui_marquee_offset(150u, 100u, 3599u) == 50);
    assert(ui_marquee_offset(150u, 100u, 4600u) == 25);
    assert(ui_marquee_offset(150u, 100u, 5600u) == 0);
    assert(ui_marquee_offset(150u, 100u, 1800u) == ui_marquee_offset(150u, 100u, 30u * 60u));

    uint8_t pixels[UI_INDEXED4_STRIDE(32u) * 10u] = {0};
    ui_canvas_t canvas = {
        .pixels = pixels,
        .width = 32u,
        .height = 10u,
        .stride_bytes = UI_INDEXED4_STRIDE(32u),
    };
    ui_canvas_set_clip(&canvas, (ui_rect_t){4, 0, 20, 10});
    const ui_rect_t saved = canvas.clip;
    ui_draw_text_marquee(&canvas, (ui_rect_t){8, 0, 12, 7}, "AAAA", 1u, UI_COLOR_TEXT, 5);
    assert(memcmp(&saved, &canvas.clip, sizeof(saved)) == 0);
    bool drew_inside = false;
    for (uint16_t y = 0u; y < 10u; ++y) {
        for (uint16_t x = 0u; x < 32u; ++x) {
            const uint8_t pixel = ui_canvas_get_pixel(&canvas, x, y);
            if (x >= 8u && x < 20u && pixel != 0u)
                drew_inside = true;
            if (x < 8u || x >= 20u)
                assert(pixel == 0u);
        }
    }
    assert(drew_inside);

    ui_library_t library = {
        .page_revision = 12u,
        .prepared_count = 1u,
        .visible = {{
            .track = {7u, 2u},
            .page_revision = 12u,
            .row = 0u,
            .state = VISIBLE_TITLE_LOADING,
        }},
    };
    const visible_title_t stale = {
        .track = {7u, 2u},
        .page_revision = 11u,
        .row = 0u,
        .state = VISIBLE_TITLE_ID666,
        .title = "STALE",
    };
    assert(!ui_library_accept_title(&library, &stale, 100u));
    assert(library.visible[0].state == VISIBLE_TITLE_LOADING);
    visible_title_t current = stale;
    current.page_revision = 12u;
    strcpy(current.title, "CURRENT TITLE");
    assert(ui_library_accept_title(&library, &current, 200u));
    assert(strcmp(library.visible[0].title, "CURRENT TITLE") == 0);
    assert(library.row_epoch_ms[0] == 200u);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    if (strcmp(argv[1], "touch") == 0) {
        test_touch_targets_and_cancellation();
    } else if (strcmp(argv[1], "commands") == 0) {
        test_command_queue_preserves_order_and_reports_full();
    } else if (strcmp(argv[1], "render") == 0) {
        test_dirty_render_matches_full_render();
    } else if (strcmp(argv[1], "marquee") == 0) {
        test_marquee_and_stale_titles();
    } else if (strcmp(argv[1], "routing") == 0) {
        test_screen_routing_and_visual_rate();
    } else {
        return 2;
    }
    puts("GUI contract OK");
    return 0;
}
