#ifndef UI_LIBRARY_H
#define UI_LIBRARY_H

#include <stdbool.h>
#include <stdint.h>

#include "storage/spc_catalog.h"
#include "ui_draw.h"

typedef enum {
    UI_LIBRARY_NO_CARD = 0,
    UI_LIBRARY_READY,
    UI_LIBRARY_LOADING,
    UI_LIBRARY_EMPTY,
    UI_LIBRARY_ERROR,
} ui_library_state_t;

typedef struct {
    const spc_library_catalog_t *catalog;
    const char *error;
    const char *current_folder;
    const char *current_filename;
    ui_library_state_t state;
    bool has_current_track;
    bool show_embedded;
    uint32_t animation_ms;
} ui_library_model_t;

typedef struct {
    uint16_t selected_folder;
    uint16_t page;
    bool tracks_visible;
    uint32_t page_revision;
    uint32_t prepared_folder_revision;
    uint16_t prepared_page;
    uint8_t prepared_count;
    visible_title_t visible[3];
    uint32_t row_epoch_ms[3];
    int16_t row_offset[3];
    bool title_request_pending;
} ui_library_t;

typedef struct {
    bool changed;
    bool folder_selected;
    bool track_selected;
    bool embedded_selected;
    uint16_t folder_index;
    spc_track_ref_t track;
} ui_library_action_t;

void ui_library_draw(ui_canvas_t *canvas, const ui_library_model_t *model,
                     const ui_library_t *library, ui_target_t pressed_target);
ui_library_action_t ui_library_activate(ui_library_t *library, const ui_library_model_t *model,
                                        ui_target_t target);
void ui_library_sync(ui_library_t *library, const ui_library_model_t *model);
bool ui_library_take_title_request(ui_library_t *library, uint32_t *page_revision,
                                   spc_track_ref_t *tracks, uint8_t *count);
bool ui_library_accept_title(ui_library_t *library, const visible_title_t *title,
                             uint32_t animation_ms);
bool ui_library_update_offsets(ui_library_t *library, const ui_library_model_t *model);

#endif
