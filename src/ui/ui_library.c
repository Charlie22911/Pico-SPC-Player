#include "ui_library.h"

#include <stddef.h>
#include <string.h>

#include "ui_marquee.h"

#define LIBRARY_ROWS 3u

static uint16_t visible_count(const ui_library_model_t *model, const ui_library_t *library) {
    if (model == NULL || model->catalog == NULL)
        return 0u;
    return library->tracks_visible ? model->catalog->track_count : model->catalog->folder_count;
}

static bool has_page(const ui_library_model_t *model, const ui_library_t *library, uint16_t page) {
    return (uint32_t)page * LIBRARY_ROWS < visible_count(model, library);
}

static void advance_page_revision(ui_library_t *library) {
    ++library->page_revision;
    if (library->page_revision == 0u)
        ++library->page_revision;
}

static void invalidate_prepared_page(ui_library_t *library) {
    library->prepared_folder_revision = UINT32_MAX;
    library->prepared_count = 0u;
    library->title_request_pending = false;
}

void ui_library_sync(ui_library_t *library, const ui_library_model_t *model) {
    if (library == NULL || model == NULL || model->catalog == NULL || !library->tracks_visible)
        return;
    if (model->state == UI_LIBRARY_LOADING) {
        /* Loading a song does not invalidate the folder being browsed. */
        if (library->prepared_folder_revision != model->catalog->folder_revision)
            invalidate_prepared_page(library);
        return;
    }
    if (library->prepared_folder_revision == model->catalog->folder_revision &&
        library->prepared_page == library->page)
        return;

    advance_page_revision(library);
    library->prepared_folder_revision = model->catalog->folder_revision;
    library->prepared_page = library->page;
    library->prepared_count = 0u;
    for (uint8_t row = 0u; row < LIBRARY_ROWS; ++row) {
        const uint32_t index = (uint32_t)library->page * LIBRARY_ROWS + row;
        if (index >= model->catalog->track_count)
            break;
        library->visible[row] = (visible_title_t){
            .track = {model->catalog->folder_revision, (uint16_t)index},
            .page_revision = library->page_revision,
            .row = row,
            .state = VISIBLE_TITLE_LOADING,
        };
        library->row_epoch_ms[row] = model->animation_ms;
        library->row_offset[row] = 0;
        ++library->prepared_count;
    }
    library->title_request_pending =
        library->prepared_count != 0u && model->state == UI_LIBRARY_READY;
}

bool ui_library_take_title_request(ui_library_t *library, uint32_t *page_revision,
                                   spc_track_ref_t *tracks, uint8_t *count) {
    if (library == NULL || page_revision == NULL || tracks == NULL || count == NULL ||
        !library->title_request_pending)
        return false;
    *page_revision = library->page_revision;
    *count = library->prepared_count;
    for (uint8_t row = 0u; row < *count; ++row)
        tracks[row] = library->visible[row].track;
    library->title_request_pending = false;
    return true;
}

bool ui_library_accept_title(ui_library_t *library, const visible_title_t *title,
                             uint32_t animation_ms) {
    if (library == NULL || title == NULL || title->row >= LIBRARY_ROWS ||
        title->row >= library->prepared_count)
        return false;
    visible_title_t *row = &library->visible[title->row];
    if (!visible_title_matches(title, library->page_revision, row->track, title->row))
        return false;
    *row = *title;
    library->row_epoch_ms[title->row] = animation_ms;
    library->row_offset[title->row] = 0;
    return true;
}

static const char *track_row_label(const ui_library_model_t *model, const ui_library_t *library,
                                   uint8_t row) {
    if (row >= library->prepared_count)
        return "";
    const visible_title_t *visible = &library->visible[row];
    if (visible->state == VISIBLE_TITLE_ID666 && visible->title[0] != '\0')
        return visible->title;
    const uint16_t index = visible->track.track_index;
    return index < model->catalog->track_count ? model->catalog->tracks[index].filename : "";
}

bool ui_library_update_offsets(ui_library_t *library, const ui_library_model_t *model) {
    if (library == NULL || model == NULL || !library->tracks_visible)
        return false;
    bool changed = false;
    for (uint8_t row = 0u; row < library->prepared_count; ++row) {
        const ui_rect_t rect =
            ui_target_rect(UI_SCREEN_LIBRARY, (ui_target_t)(UI_TARGET_LIBRARY_ROW_1 + row));
        const uint16_t viewport_width = rect.width > 20 ? (uint16_t)(rect.width - 20) : 0u;
        const int16_t offset =
            ui_marquee_offset(ui_text_width(track_row_label(model, library, row), 3u),
                              viewport_width, model->animation_ms - library->row_epoch_ms[row]);
        if (offset != library->row_offset[row]) {
            library->row_offset[row] = offset;
            changed = true;
        }
    }
    return changed;
}

static void draw_folder_row(ui_canvas_t *canvas, uint8_t row, const char *label, bool pressed) {
    char shown[23];
    size_t length = strlen(label);
    if (length >= sizeof(shown))
        length = sizeof(shown) - 1u;
    memcpy(shown, label, length);
    shown[length] = '\0';
    ui_draw_button(canvas,
                   ui_target_rect(UI_SCREEN_LIBRARY, (ui_target_t)(UI_TARGET_LIBRARY_ROW_1 + row)),
                   shown, false, pressed);
}

static void draw_track_row(ui_canvas_t *canvas, const ui_library_model_t *model,
                           const ui_library_t *library, uint8_t row, bool pressed) {
    if (row >= library->prepared_count)
        return;
    const visible_title_t *visible = &library->visible[row];
    const char *label = track_row_label(model, library, row);
    const ui_rect_t rect =
        ui_target_rect(UI_SCREEN_LIBRARY, (ui_target_t)(UI_TARGET_LIBRARY_ROW_1 + row));
    const uint16_t index = visible->track.track_index;
    const bool selected =
        model->has_current_track && index < model->catalog->track_count &&
        strcmp(model->current_filename, model->catalog->tracks[index].filename) == 0 &&
        library->selected_folder < model->catalog->folder_count &&
        strcmp(model->current_folder, model->catalog->folders[library->selected_folder].name) == 0;
    ui_draw_fill(canvas, rect, pressed ? UI_COLOR_ACCENT : UI_COLOR_PANEL);
    ui_draw_outline(canvas, rect, selected ? UI_COLOR_ACCENT : UI_COLOR_BORDER, 2u);
    const uint8_t color = pressed ? UI_COLOR_BACKGROUND : UI_COLOR_TEXT;
    const uint16_t width = ui_text_width(label, 3u);
    const ui_rect_t viewport = {(int16_t)(rect.x + 10), (int16_t)(rect.y + (rect.height - 21) / 2),
                                (int16_t)(rect.width - 20), 21};
    if (width <= (uint16_t)viewport.width) {
        ui_draw_text(canvas, (int16_t)(rect.x + (rect.width - (int16_t)width) / 2), viewport.y,
                     label, 3u, color);
    } else {
        ui_draw_text_marquee(canvas, viewport, label, 3u, color, library->row_offset[row]);
    }
}

void ui_library_draw(ui_canvas_t *canvas, const ui_library_model_t *model,
                     const ui_library_t *library, ui_target_t pressed_target) {
    ui_draw_text(canvas, 12, 108, library->tracks_visible ? "TRACKS" : "LIBRARY", 3u,
                 UI_COLOR_TEXT);
    if (model->show_embedded) {
        ui_draw_button(canvas, ui_target_rect(UI_SCREEN_LIBRARY, UI_TARGET_LIBRARY_ROW_1),
                       "EMBEDDED SPC", model->has_current_track,
                       pressed_target == UI_TARGET_LIBRARY_ROW_1);
        ui_draw_text(canvas, 12, 252, "INSERT A FAT32 CARD", 2u, UI_COLOR_SECONDARY);
        ui_draw_text(canvas, 12, 280, "WITH /GAME/TRACK.SPC", 2u, UI_COLOR_SECONDARY);
        return;
    }

    const uint16_t count = visible_count(model, library);
    if (count == 0u) {
        const char *message = model->state == UI_LIBRARY_LOADING ? "SCANNING..."
                              : model->error == NULL
                                  ? (library->tracks_visible ? "NO SPC FILES" : "NO GAME FOLDERS")
                                  : model->error;
        ui_draw_text_clipped(canvas, (ui_rect_t){12, 178, 426, 24}, message, 3u,
                             model->state == UI_LIBRARY_LOADING ? UI_COLOR_ACCENT : UI_COLOR_ERROR);
        ui_draw_text(canvas, 12, 224, "EXPECTED /GAME/TRACK.SPC", 2u, UI_COLOR_SECONDARY);
    } else {
        for (uint8_t row = 0u; row < LIBRARY_ROWS; ++row) {
            const uint32_t index = (uint32_t)library->page * LIBRARY_ROWS + row;
            if (index >= count)
                continue;
            const bool pressed = pressed_target == (ui_target_t)(UI_TARGET_LIBRARY_ROW_1 + row);
            if (library->tracks_visible) {
                draw_track_row(canvas, model, library, row, pressed);
            } else {
                draw_folder_row(canvas, row, model->catalog->folders[index].name, pressed);
            }
        }
    }
    if (library->tracks_visible) {
        ui_draw_button(canvas, ui_target_rect(UI_SCREEN_LIBRARY, UI_TARGET_LIBRARY_BACK), "BACK",
                       false, pressed_target == UI_TARGET_LIBRARY_BACK);
    }
    if (library->page != 0u) {
        ui_draw_button(canvas, ui_target_rect(UI_SCREEN_LIBRARY, UI_TARGET_LIBRARY_PREV), "PREV",
                       false, pressed_target == UI_TARGET_LIBRARY_PREV);
    }
    if (has_page(model, library, (uint16_t)(library->page + 1u))) {
        ui_draw_button(canvas, ui_target_rect(UI_SCREEN_LIBRARY, UI_TARGET_LIBRARY_NEXT), "NEXT",
                       false, pressed_target == UI_TARGET_LIBRARY_NEXT);
    }
    if (model->state == UI_LIBRARY_LOADING) {
        ui_draw_text(canvas, 306, 116, "LOADING", 2u, UI_COLOR_ACCENT);
    } else if (model->error != NULL) {
        ui_draw_text_clipped(canvas, (ui_rect_t){246, 116, 192, 16}, model->error, 2u,
                             UI_COLOR_ERROR);
    }
}

ui_library_action_t ui_library_activate(ui_library_t *library, const ui_library_model_t *model,
                                        ui_target_t target) {
    ui_library_action_t action = {0};
    if (library == NULL || model == NULL || model->catalog == NULL)
        return action;
    if (model->show_embedded && target == UI_TARGET_LIBRARY_ROW_1) {
        action.embedded_selected = true;
        return action;
    }
    if (target == UI_TARGET_LIBRARY_BACK && library->tracks_visible) {
        library->tracks_visible = false;
        library->page = 0u;
        invalidate_prepared_page(library);
        action.changed = true;
        return action;
    }
    if (target == UI_TARGET_LIBRARY_PREV && library->page != 0u) {
        --library->page;
        invalidate_prepared_page(library);
        ui_library_sync(library, model);
        action.changed = true;
        return action;
    }
    if (target == UI_TARGET_LIBRARY_NEXT) {
        if (has_page(model, library, (uint16_t)(library->page + 1u))) {
            ++library->page;
            invalidate_prepared_page(library);
            ui_library_sync(library, model);
            action.changed = true;
        }
        return action;
    }
    if (target < UI_TARGET_LIBRARY_ROW_1 || target > UI_TARGET_LIBRARY_ROW_3 ||
        model->state == UI_LIBRARY_LOADING)
        return action;
    const uint16_t row = (uint16_t)(target - UI_TARGET_LIBRARY_ROW_1);
    const uint32_t index = (uint32_t)library->page * LIBRARY_ROWS + row;
    if (index >= visible_count(model, library))
        return action;
    if (library->tracks_visible) {
        action.track_selected = true;
        action.track = (spc_track_ref_t){model->catalog->folder_revision, (uint16_t)index};
    } else {
        library->selected_folder = (uint16_t)index;
        library->tracks_visible = true;
        library->page = 0u;
        invalidate_prepared_page(library);
        action.changed = true;
        action.folder_selected = true;
        action.folder_index = (uint16_t)index;
    }
    return action;
}
