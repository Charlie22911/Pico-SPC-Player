#include "ui/ui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4)
        return 2;
    uint8_t *pixels = calloc(UI_INDEXED4_STRIDE(450u) * 600u, sizeof(*pixels));
    if (pixels == NULL)
        return 1;
    ui_canvas_t canvas = {
        .pixels = pixels,
        .width = 450u,
        .height = 600u,
        .stride_bytes = UI_INDEXED4_STRIDE(450u),
    };
    static const spc_metadata_t metadata = {
        .has_id666 = true,
        .title = "Cast List (Looped)",
        .game = "Super Mario World",
        .artist = "Koji Kondo",
        .dumper = "KungFuFurby",
    };
    spc_snapshot_t snapshot = {
        .sequence = 1u,
        .generation = 1u,
        .validity = SPC_SNAPSHOT_VALID_DSP_REGISTERS | SPC_SNAPSHOT_VALID_VOICE_INTERNALS,
    };
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
    for (uint16_t i = 0u; i < 128u; ++i) {
        snapshot.dsp_registers[i] = (uint8_t)i;
    }
    static uint8_t aram_read[ARAM_ACTIVITY_BITMAP_BYTES];
    static uint8_t aram_write[ARAM_ACTIVITY_BITMAP_BYTES];
    static uint8_t aram_execute[ARAM_ACTIVITY_BITMAP_BYTES];
    for (uint32_t address = 0u; address < ARAM_ACTIVITY_ADDRESS_COUNT; ++address) {
        const uint8_t x = (uint8_t)address;
        const uint8_t y = (uint8_t)(address >> 8u);
        if ((uint8_t)(x - y) < 3u) {
            aram_activity_mark(aram_read, (uint16_t)address);
        }
        if ((uint8_t)(x + y) < 3u) {
            aram_activity_mark(aram_write, (uint16_t)address);
        }
        if ((x & 31u) == 0u && (y & 31u) == 0u) {
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
    const ui_model_t model = {
        .player_ready = true,
        .audio_ready = true,
        .paused = false,
        .touch_ready = true,
        .volume_step = 8u,
        .generation = 1u,
        .silence_frames = 0u,
        .track_frames = 83u * 32000u,
        .peak_left = 28000u,
        .peak_right = 19000u,
        .metadata = &metadata,
        .spc_snapshot = &snapshot,
        .snapshot_sequence = snapshot.sequence,
        .aram_view = &aram_view,
        .aram_sequence = aram_view.sequence,
    };
    ui_t ui;
    ui_init(&ui, &model);
    if (argc >= 3) {
        const unsigned long screen = strtoul(argv[2], NULL, 10);
        if (screen > (unsigned long)UI_SCREEN_VOLUME) {
            free(pixels);
            return 2;
        }
        ui.screen = (ui_screen_t)screen;
    }
    if (argc == 4) {
        ui.dsp_page = (uint8_t)(strtoul(argv[3], NULL, 10) != 0u);
    }
    ui_render(&ui, &canvas);

    FILE *output = fopen(argv[1], "wb");
    if (output == NULL) {
        free(pixels);
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
            const uint16_t pixel =
                ui_palette_rgb565[ui_canvas_get_pixel(&canvas, (uint16_t)x, (uint16_t)y)];
            fputc((int)((pixel & 31u) * 255u / 31u), output);
            fputc((int)(((pixel >> 5u) & 63u) * 255u / 63u), output);
            fputc((int)(((pixel >> 11u) & 31u) * 255u / 31u), output);
        }
        fputc(0, output);
        fputc(0, output);
    }
    const bool ok = fclose(output) == 0;
    free(pixels);
    return ok ? 0 : 1;
}
