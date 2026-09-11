#include "display.h"

#include <stddef.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "AMOLED_2in41.h"
#include "DEV_Config.h"
#include "qspi_pio.h"

#define DISPLAY_STAGE_ROWS 8u
#define DISPLAY_PAIRS_PER_ROW (AMOLED_2IN41_WIDTH / 2u)
#define DISPLAY_DMA_TIMEOUT_US 50000u
#define DISPLAY_FIFO_TIMEOUT_US 20000u

typedef enum {
    DISPLAY_PHASE_IDLE = 0,
    DISPLAY_PHASE_DMA,
    DISPLAY_PHASE_FIFO,
} display_phase_t;

typedef struct {
    const uint8_t *pixels;
    uint16_t stride_bytes;
    uint16_t width;
    uint16_t height;
    uint16_t next_row;
    uint16_t completed_rows;
    uint16_t buffer_rows[2];
    bool buffer_ready[2];
    int8_t in_flight;
    display_phase_t phase;
    absolute_time_t deadline;
    uint32_t started_us;
    bool aram_map;
    uint16_t origin_x;
    uint16_t origin_y;
    display_palette_region_t secondary_regions[DISPLAY_PALETTE_REGION_CAPACITY];
    uint8_t secondary_region_count;
} display_transfer_t;

static uint32_t pair_lut[256];
static uint32_t secondary_pair_lut[256];
static uint32_t stage[2][DISPLAY_PAIRS_PER_ROW * DISPLAY_STAGE_ROWS];
static display_transfer_t transfer;
static display_stats_t stats;
static uint32_t aram_window_started_us;
static uint32_t aram_window_maps;

static uint16_t wire_color(uint16_t color) {
    return (uint16_t)(color >> 8u | color << 8u);
}

static void build_pair_lut(uint32_t lut[256], const uint16_t palette[16]) {
    if (palette == NULL)
        return;
    uint16_t wire[16];
    for (uint32_t i = 0u; i < 16u; ++i)
        wire[i] = wire_color(palette[i]);
    for (uint32_t packed = 0u; packed < 256u; ++packed) {
        const uint16_t left = wire[packed >> 4u];
        const uint16_t right = wire[packed & 0x0fu];
        lut[packed] = (uint32_t)left | (uint32_t)right << 16u;
    }
}

void display_set_palette(const uint16_t palette[16]) {
    build_pair_lut(pair_lut, palette);
}

void display_set_secondary_palette(const uint16_t palette[16]) {
    build_pair_lut(secondary_pair_lut, palette);
}

static bool pair_uses_secondary_palette(uint16_t x, uint16_t y) {
    for (uint8_t i = 0u; i < transfer.secondary_region_count; ++i) {
        const display_palette_region_t *region = &transfer.secondary_regions[i];
        if (x >= region->x && x < (uint32_t)region->x + region->width && y >= region->y &&
            y < (uint32_t)region->y + region->height) {
            return true;
        }
    }
    return false;
}

static void recover_transport(void) {
    dma_channel_abort(g_dma_tx_channel);
    pio_sm_set_enabled(g_qspi.pio, g_qspi.sm, false);
    pio_sm_clear_fifos(g_qspi.pio, g_qspi.sm);
    pio_sm_restart(g_qspi.pio, g_qspi.sm);
    pio_sm_set_enabled(g_qspi.pio, g_qspi.sm, true);
    gpio_put(g_qspi.pin_cs, 1);
    transfer = (display_transfer_t){0};
    transfer.in_flight = -1;
}

static bool prepare_buffer(uint32_t buffer) {
    if (transfer.next_row >= transfer.height || transfer.buffer_ready[buffer] ||
        transfer.in_flight == (int8_t)buffer) {
        return false;
    }
    uint16_t rows = (uint16_t)(transfer.height - transfer.next_row);
    if (rows > DISPLAY_STAGE_ROWS)
        rows = DISPLAY_STAGE_ROWS;
    const uint16_t pairs = transfer.width / 2u;
    for (uint16_t row = 0u; row < rows; ++row) {
        const uint8_t *source =
            transfer.pixels + (size_t)(transfer.next_row + row) * transfer.stride_bytes;
        uint32_t *destination = stage[buffer] + (size_t)row * pairs;
        for (uint16_t pair = 0u; pair < pairs; ++pair) {
            const uint16_t x = (uint16_t)(transfer.origin_x + pair * 2u);
            const uint16_t y = (uint16_t)(transfer.origin_y + transfer.next_row + row);
            const uint32_t *lut = pair_uses_secondary_palette(x, y) ? secondary_pair_lut : pair_lut;
            destination[pair] = lut[source[pair]];
        }
    }
    transfer.buffer_rows[buffer] = rows;
    transfer.buffer_ready[buffer] = true;
    transfer.next_row = (uint16_t)(transfer.next_row + rows);
    return true;
}

static void start_buffer(uint32_t buffer) {
    transfer.buffer_ready[buffer] = false;
    transfer.in_flight = (int8_t)buffer;
    dma_channel_configure(
        g_dma_tx_channel, &g_dma_tx_config, &g_qspi.pio->txf[g_qspi.sm], stage[buffer],
        (uint32_t)transfer.buffer_rows[buffer] * transfer.width * sizeof(uint16_t), true);
    transfer.phase = DISPLAY_PHASE_DMA;
    transfer.deadline = make_timeout_time_us(DISPLAY_DMA_TIMEOUT_US);
}

bool display_begin_indexed4(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                            const uint8_t *pixels, uint16_t stride_bytes,
                            const display_palette_region_t *secondary_regions,
                            uint8_t secondary_region_count) {
    if (get_core_num() != 1u || transfer.phase != DISPLAY_PHASE_IDLE ||
        dma_channel_is_busy(g_dma_tx_channel) || pixels == NULL || width == 0u || height == 0u ||
        (x & 1u) != 0u || (width & 1u) != 0u || width > AMOLED_2IN41_WIDTH ||
        stride_bytes < width / 2u || (uint32_t)x + width > AMOLED_2IN41_WIDTH ||
        (uint32_t)y + height > AMOLED_2IN41_HEIGHT ||
        secondary_region_count > DISPLAY_PALETTE_REGION_CAPACITY ||
        (secondary_region_count != 0u && secondary_regions == NULL)) {
        return false;
    }
    for (uint8_t i = 0u; i < secondary_region_count; ++i) {
        if ((secondary_regions[i].x & 1u) != 0u || (secondary_regions[i].width & 1u) != 0u)
            return false;
    }

    transfer = (display_transfer_t){
        .pixels = pixels,
        .stride_bytes = stride_bytes,
        .width = width,
        .height = height,
        .in_flight = -1,
        .started_us = time_us_32(),
        .origin_x = x,
        .origin_y = y,
        .secondary_region_count = secondary_region_count,
    };
    for (uint8_t i = 0u; i < secondary_region_count; ++i)
        transfer.secondary_regions[i] = secondary_regions[i];
    (void)prepare_buffer(0u);
    AMOLED_2IN41_SetWindows(x, y, (uint32_t)x + width, (uint32_t)y + height);
    QSPI_Select(g_qspi);
    QSPI_Pixel_Write(g_qspi, 0x2c);
    channel_config_set_transfer_data_size(&g_dma_tx_config, DMA_SIZE_8);
    channel_config_set_read_increment(&g_dma_tx_config, true);
    channel_config_set_write_increment(&g_dma_tx_config, false);
    channel_config_set_high_priority(&g_dma_tx_config, false);
    channel_config_set_dreq(&g_dma_tx_config, pio_get_dreq(g_qspi.pio, g_qspi.sm, true));
    start_buffer(0u);
    (void)prepare_buffer(1u);
    return true;
}

display_transfer_status_t display_poll(void) {
    if (transfer.phase == DISPLAY_PHASE_IDLE)
        return DISPLAY_TRANSFER_IDLE;
    if (transfer.phase == DISPLAY_PHASE_DMA) {
        if (dma_channel_is_busy(g_dma_tx_channel)) {
            const uint32_t available = transfer.in_flight == 0 ? 1u : 0u;
            (void)prepare_buffer(available);
            if (time_reached(transfer.deadline)) {
                recover_transport();
                return DISPLAY_TRANSFER_ERROR;
            }
            return DISPLAY_TRANSFER_ACTIVE;
        }

        const uint32_t finished = (uint32_t)transfer.in_flight;
        transfer.completed_rows =
            (uint16_t)(transfer.completed_rows + transfer.buffer_rows[finished]);
        transfer.in_flight = -1;
        const uint32_t next = finished ^ 1u;
        if (transfer.buffer_ready[next]) {
            start_buffer(next);
            (void)prepare_buffer(finished);
            return DISPLAY_TRANSFER_ACTIVE;
        }
        if (transfer.next_row < transfer.height) {
            (void)prepare_buffer(finished);
            start_buffer(finished);
            return DISPLAY_TRANSFER_ACTIVE;
        }
        transfer.phase = DISPLAY_PHASE_FIFO;
        transfer.deadline = make_timeout_time_us(DISPLAY_FIFO_TIMEOUT_US);
    }

    if (transfer.phase == DISPLAY_PHASE_FIFO) {
        if (!pio_sm_is_tx_fifo_empty(g_qspi.pio, g_qspi.sm)) {
            if (time_reached(transfer.deadline)) {
                recover_transport();
                return DISPLAY_TRANSFER_ERROR;
            }
            return DISPLAY_TRANSFER_ACTIVE;
        }
        busy_wait_us_32(2u);
        gpio_put(g_qspi.pin_cs, 1);
        const uint32_t now = time_us_32();
        const uint32_t elapsed = now - transfer.started_us;
        stats.last_transfer_us = elapsed;
        if (elapsed > stats.max_transfer_us)
            stats.max_transfer_us = elapsed;
        ++stats.completed_transfers;
        if (transfer.aram_map) {
            ++stats.completed_aram_maps;
            const uint32_t window_us = now - aram_window_started_us;
            if (aram_window_started_us == 0u || window_us > 1500000u) {
                aram_window_started_us = now;
                aram_window_maps = 0u;
            } else {
                ++aram_window_maps;
                if (window_us >= 1000000u) {
                    const uint32_t rate_x10 = aram_window_maps * 10000000u / window_us;
                    stats.measured_aram_hz_x10 =
                        rate_x10 > UINT16_MAX ? UINT16_MAX : (uint16_t)rate_x10;
                    aram_window_maps = 0u;
                    aram_window_started_us = now;
                }
            }
        }
        transfer = (display_transfer_t){0};
        transfer.in_flight = -1;
        return DISPLAY_TRANSFER_COMPLETE;
    }

    recover_transport();
    return DISPLAY_TRANSFER_ERROR;
}

bool display_transfer_active(void) {
    return transfer.phase != DISPLAY_PHASE_IDLE;
}

void display_mark_current_transfer_aram(void) {
    if (get_core_num() == 1u && transfer.phase != DISPLAY_PHASE_IDLE)
        transfer.aram_map = true;
}

display_stats_t display_get_stats(void) {
    return stats;
}

void display_cancel(void) {
    if (display_transfer_active() || dma_channel_is_busy(g_dma_tx_channel)) {
        recover_transport();
    }
}
