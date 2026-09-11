#include "ui/ui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spc/spc_metadata.h"
#include "spc/software_spc_backend.h"
#include "ui/ui_aram.h"

static void write_u16(FILE *file, uint16_t value) {
    fputc((int)(value & 0xffu), file);
    fputc((int)((value >> 8u) & 0xffu), file);
}

static void write_u32(FILE *file, uint32_t value) {
    fputc((int)(value & 0xffu), file);
    fputc((int)((value >> 8u) & 0xffu), file);
    fputc((int)((value >> 16u) & 0xffu), file);
    fputc((int)((value >> 24u) & 0xffu), file);
}

static uint8_t *load_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL)
            fclose(file);
        return NULL;
    }
    const long length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    uint8_t *data = malloc((size_t)length);
    if (data == NULL || fread(data, 1u, (size_t)length, file) != (size_t)length) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (size_t)length;
    return data;
}

static const char *filename_from_path(const char *path) {
    const char *filename = path;
    for (const char *at = path; *at != '\0'; ++at) {
        if (*at == '/' || *at == '\\')
            filename = at + 1;
    }
    return filename;
}

static uint8_t heatmap_index(uint8_t value) {
    return value == 0u ? 0u : (uint8_t)(1u + (uint8_t)(value - 1u) / 17u);
}

static void apply_aram_data(aram_view_t *view, const uint8_t *packed_data) {
    for (uint32_t address = 0u; address < 65536u; address += 2u) {
        view->pixels[address >> 1u] =
            packed_data != NULL ? packed_data[address >> 1u]
                                : (uint8_t)(heatmap_index((uint8_t)(address >> 3u)) << 4u |
                                            heatmap_index((uint8_t)(address >> 5u)));
    }
    view->generation = 1u;
    view->sequence = 1u;
    view->request = 1u;
    view->kind = ARAM_VIEW_DATA;
    view->valid = true;
}

static bool capture_spc_state(const uint8_t *spc_image, size_t spc_size, uint8_t *read,
                              uint8_t *write, uint8_t *execute, spc_snapshot_t *snapshot,
                              uint8_t *packed_data, uint16_t *peak_left, uint16_t *peak_right) {
    software_spc_backend_t *backend = software_spc_create();
    if (backend == NULL)
        return false;
    bool ok = software_spc_load(backend, spc_image, spc_size, 1) == NULL;
    software_spc_set_aram_visualizer(backend, read, write, execute);
    int16_t samples[256u * SOFTWARE_SPC_CHANNELS];
    for (uint32_t block = 0u; ok && block < 64u; ++block) {
        ok = software_spc_render(backend, samples, 256u) == NULL;
        for (size_t frame = 0u; ok && frame < 256u; ++frame) {
            int left = samples[frame * 2u];
            int right = samples[frame * 2u + 1u];
            left = left < 0 ? -left : left;
            right = right < 0 ? -right : right;
            if ((uint16_t)left > *peak_left)
                *peak_left = (uint16_t)left;
            if ((uint16_t)right > *peak_right)
                *peak_right = (uint16_t)right;
        }
    }
    ok = ok && software_spc_capture_snapshot(backend, 1u, 1u, 0u, snapshot) &&
         software_spc_capture_aram_heatmap(backend, packed_data);
    software_spc_destroy(backend);
    return ok;
}

static bool inside(ui_rect_t rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 5)
        return 2;
    const unsigned long screen = argc >= 3 ? strtoul(argv[2], NULL, 10) : 0u;
    const unsigned long variant = argc >= 4 ? strtoul(argv[3], NULL, 10) : 0u;
    if (screen > (unsigned long)UI_SCREEN_VOLUME)
        return 2;

    size_t spc_size = 0u;
    uint8_t *spc_image = argc == 5 ? load_file(argv[4], &spc_size) : NULL;
    if (argc == 5 && spc_image == NULL)
        return 1;

    uint8_t *pixels = calloc(UI_INDEXED4_STRIDE(450u) * 600u, sizeof(*pixels));
    if (pixels == NULL) {
        free(spc_image);
        return 1;
    }
    ui_canvas_t canvas = {
        .pixels = pixels,
        .width = 450u,
        .height = 600u,
        .stride_bytes = UI_INDEXED4_STRIDE(450u),
    };
    spc_metadata_t metadata = {
        .has_id666 = true,
        .title = "Cast List (Looped)",
        .game = "Super Mario World",
        .artist = "Koji Kondo",
        .dumper = "KungFuFurby",
    };
    if (spc_image != NULL && !spc_metadata_parse(spc_image, spc_size, &metadata)) {
        free(pixels);
        free(spc_image);
        return 1;
    }
    spc_snapshot_t snapshot = {
        .sequence = 1u,
        .generation = 1u,
        .validity = SPC_SNAPSHOT_VALID_DSP_REGISTERS | SPC_SNAPSHOT_VALID_VOICE_INTERNALS,
    };
    static uint8_t aram_read[ARAM_ACTIVITY_BITMAP_BYTES];
    static uint8_t aram_write[ARAM_ACTIVITY_BITMAP_BYTES];
    static uint8_t aram_execute[ARAM_ACTIVITY_BITMAP_BYTES];
    static uint8_t aram_data[ARAM_DATA_PIXELS_BYTES];
    uint16_t peak_left = 28000u;
    uint16_t peak_right = 19000u;
    if (spc_image != NULL) {
        peak_left = 0u;
        peak_right = 0u;
        if (!capture_spc_state(spc_image, spc_size, aram_read, aram_write, aram_execute, &snapshot,
                               aram_data, &peak_left, &peak_right)) {
            free(pixels);
            free(spc_image);
            return 1;
        }
    } else {
        for (uint8_t voice = 0u; voice < 8u; ++voice) {
            snapshot.voices[voice] = (spc_voice_snapshot_t){
                .volume_left = (int8_t)(64 - voice * 8),
                .volume_right = (int8_t)(48 - voice * 6),
                .pitch = (uint16_t)(0x0800u + voice * 0x0120u),
                .envelope = (uint16_t)(0x180u + voice * 0xc0u),
                .brr_address = (uint16_t)(0x4000u + voice * 0x180u),
                .source_number = (uint8_t)(voice + 2u),
                .adsr0 = 0x8fu,
                .adsr1 = 0xe0u,
                .gain = 0x7fu,
                .envx = (uint8_t)(0x18u + voice * 8u),
                .outx = (int8_t)(voice * 12 - 36),
                .envelope_mode = (uint8_t)(voice & 3u),
            };
        }
        for (uint16_t i = 0u; i < 128u; ++i)
            snapshot.dsp_registers[i] = (uint8_t)i;
        for (uint32_t address = 0u; address < ARAM_ACTIVITY_ADDRESS_COUNT; ++address) {
            const uint8_t x = (uint8_t)address;
            const uint8_t y = (uint8_t)(address >> 8u);
            if ((uint8_t)(x - y) < 3u)
                aram_activity_mark(aram_read, (uint16_t)address);
            if ((uint8_t)(x + y) < 3u)
                aram_activity_mark(aram_write, (uint16_t)address);
            if ((x & 31u) == 0u && (y & 31u) == 0u)
                aram_activity_mark(aram_execute, (uint16_t)address);
        }
    }
    aram_activity_snapshot_t activity_snapshot = {
        .read = aram_read,
        .write = aram_write,
        .execute = aram_execute,
        .generation = 1u,
        .sequence = 1u,
        .held = true,
    };
    aram_view_t aram_view;
    aram_view_init(&aram_view);
    aram_view_apply(&aram_view, &activity_snapshot);
    if (screen == (unsigned long)UI_SCREEN_ARAM && variant != 0u)
        apply_aram_data(&aram_view, spc_image != NULL ? aram_data : NULL);

    spc_library_catalog_t catalog;
    spc_library_init(&catalog);
    (void)spc_library_add_folder(&catalog, "Chrono Trigger", false);
    (void)spc_library_add_folder(&catalog, "Donkey Kong Country 2", false);
    (void)spc_library_add_folder(&catalog, "Super Mario World", false);
    (void)spc_library_add_folder(&catalog, "The Legend of Zelda", false);
    spc_library_sort_folders(&catalog);
    spc_library_begin_folder(&catalog, 0u);
    (void)spc_library_add_track(&catalog, "001 - Presentiment.spc", SPC_CORE_LOAD_BYTES, false,
                                false);
    (void)spc_library_add_track(&catalog, "004 - Corridors of Time.spc", SPC_CORE_LOAD_BYTES, false,
                                false);
    (void)spc_library_add_track(&catalog, "009 - Wind Scene.spc", SPC_CORE_LOAD_BYTES, false,
                                false);
    spc_library_sort_tracks(&catalog);

    const char *source_file = spc_image == NULL ? "Super Mario World - Cast List (Looped).spc"
                                                : filename_from_path(argv[4]);
    const ui_model_t model = {
        .player_ready = true,
        .has_track = true,
        .player_mode = PLAYER_PLAYING,
        .audio_ready = true,
        .paused = false,
        .touch_ready = true,
        .volume_step = 8u,
        .sys_clock_hz = 250000000u,
        .generation = 1u,
        .silence_frames = 0u,
        .underrun_blocks = 0u,
        .late_dma_blocks = 0u,
        .dropped_snapshots = 0u,
        .dropped_visual_snapshots = 0u,
        .min_buffered_frames = 1536u,
        .max_render_us = 580u,
        .max_gui_slice_us = 840u,
        .track_frames = 83u * 32000u,
        .peak_left = peak_left,
        .peak_right = peak_right,
        .metadata = &metadata,
        .spc_snapshot = &snapshot,
        .snapshot_sequence = snapshot.sequence,
        .aram_view = &aram_view,
        .aram_sequence = aram_view.sequence,
        .source_file = source_file,
        .source_game = metadata.game,
        .source_embedded = false,
        .measured_aram_hz_x10 = 600u,
        .visualizer_hz = 60u,
        .library =
            {
                .catalog = &catalog,
                .current_folder = "Chrono Trigger",
                .current_filename = "004 - Corridors of Time.spc",
                .state = UI_LIBRARY_READY,
                .has_current_track = true,
            },
    };
    ui_t ui;
    ui_init(&ui, &model);
    ui.screen = (ui_screen_t)screen;
    if (ui.screen == UI_SCREEN_ARAM && variant != 0u) {
        ui.aram_data_mode = true;
        ui.aram_mode_request = 1u;
    } else if (ui.screen == UI_SCREEN_DSP) {
        ui.dsp_page = (uint8_t)(variant != 0u);
    } else if (ui.screen == UI_SCREEN_LIBRARY && variant != 0u) {
        ui.library.selected_folder = 0u;
        ui.library.tracks_visible = true;
        ui_library_sync(&ui.library, &ui.model.library);
        static const char *titles[3] = {"Presentiment", "Corridors of Time", "Wind Scene"};
        for (uint8_t row = 0u; row < ui.library.prepared_count; ++row) {
            ui.library.visible[row].state = VISIBLE_TITLE_ID666;
            (void)strcpy(ui.library.visible[row].title, titles[row]);
        }
    }
    ui_render(&ui, &canvas);

    FILE *output = fopen(argv[1], "wb");
    if (output == NULL) {
        free(pixels);
        free(spc_image);
        return 1;
    }
    const uint32_t row_bytes = 1352u;
    const uint32_t image_bytes = row_bytes * 600u;
    fputc('B', output);
    fputc('M', output);
    write_u32(output, 54u + image_bytes);
    write_u32(output, 0u);
    write_u32(output, 54u);
    write_u32(output, 40u);
    write_u32(output, 450u);
    write_u32(output, 600u);
    write_u16(output, 1u);
    write_u16(output, 24u);
    write_u32(output, 0u);
    write_u32(output, image_bytes);
    write_u32(output, 2835u);
    write_u32(output, 2835u);
    write_u32(output, 0u);
    write_u32(output, 0u);

    for (int y = 599; y >= 0; --y) {
        for (int x = 0; x < 450; ++x) {
            const uint8_t index = ui_canvas_get_pixel(&canvas, (uint16_t)x, (uint16_t)y);
            const bool data_palette =
                ui.screen == UI_SCREEN_ARAM && ui.aram_data_mode &&
                (inside(UI_ARAM_LARGE_MAP, x, y) || inside(UI_ARAM_DATA_SCALE, x, y));
            const uint16_t pixel =
                data_palette ? ui_aram_data_palette_rgb565[index] : ui_palette_rgb565[index];
            fputc((int)((pixel & 31u) * 255u / 31u), output);
            fputc((int)(((pixel >> 5u) & 63u) * 255u / 63u), output);
            fputc((int)(((pixel >> 11u) & 31u) * 255u / 31u), output);
        }
        fputc(0, output);
        fputc(0, output);
    }
    const bool ok = fclose(output) == 0;
    free(pixels);
    free(spc_image);
    return ok ? 0 : 1;
}
