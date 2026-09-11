#include "i2s_output.h"

#include <stdatomic.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#include "audio_status_queue.h"
#include "i2s_tx.pio.h"
#include "pcm_format.h"

#define I2S_PIO pio1
#define I2S_PIN_BCLK 26u
#define I2S_PIN_LRCLK 27u
#define I2S_PIN_DATA 28u
#define AUDIO_STATUS_INTERVAL_BLOCKS 4u

static pcm_queue_t *audio_queue;
static int audio_dma[2] = {-1, -1};
static int audio_sm = -1;
static uint audio_program_offset;
static uint32_t audio_input[PCM_BLOCK_FRAMES];
static uint32_t audio_blocks[2][PCM_BLOCK_FRAMES * 2u];
static pcm_block_info_t audio_block_info[2];
static pcm_block_info_t last_filled_info;
static pcm_block_info_t last_played_info;
static audio_status_queue_t status_queue;
static audio_status_snapshot_t ui_status;
static uint32_t completed_frames;
static uint32_t silence_frames;
static uint32_t underrun_blocks;
static uint32_t late_dma_blocks;
static uint32_t dropped_snapshots;
static uint32_t min_buffered_frames;
static uint8_t expected_dma_block;
static _Atomic uint32_t producer_max_render_us;

static void publish_status_snapshot(void) {
    const audio_status_snapshot_t snapshot = {
        .completed_frames = completed_frames,
        .silence_frames = silence_frames,
        .underrun_blocks = underrun_blocks,
        .late_dma_blocks = late_dma_blocks,
        .dropped_snapshots = dropped_snapshots,
        .min_buffered_frames = min_buffered_frames == UINT32_MAX ? 0u : min_buffered_frames,
        .max_render_us = atomic_load_explicit(&producer_max_render_us, memory_order_relaxed),
        .generation = last_played_info.generation,
        .track_frames = last_played_info.track_frame_end,
        .peak_left = last_played_info.peak_left,
        .peak_right = last_played_info.peak_right,
        .paused = last_played_info.paused != 0u,
        .has_track = last_played_info.generation != 0u,
        .mode = last_played_info.mode,
        .volume_step = last_played_info.volume_step,
    };
    if (!audio_status_queue_push(&status_queue, &snapshot)) {
        ++dropped_snapshots;
    }
}

static bool audio_fill_block(uint block) {
    pcm_block_info_t info;
    bool underrun = false;
    if (pcm_queue_read_block(audio_queue, audio_input, &info)) {
        last_filled_info = info;
        const uint32_t buffered = (uint32_t)pcm_queue_available(audio_queue);
        if (buffered < min_buffered_frames)
            min_buffered_frames = buffered;
    } else {
        memset(audio_input, 0, sizeof(audio_input));
        silence_frames += PCM_BLOCK_FRAMES;
        ++underrun_blocks;
        underrun = true;
        info = last_filled_info;
        info.peak_left = 0u;
        info.peak_right = 0u;
        min_buffered_frames = 0u;
    }
    audio_block_info[block] = info;
    for (uint i = 0; i < PCM_BLOCK_FRAMES; ++i) {
        const uint32_t frame = audio_input[i];
        audio_blocks[block][i * 2u] = frame & 0xffff0000u;
        audio_blocks[block][i * 2u + 1u] = frame << 16u;
    }
    return underrun;
}

static void rearm_block(uint block) {
    dma_channel_set_read_addr((uint)audio_dma[block], audio_blocks[block], false);
    dma_channel_set_trans_count((uint)audio_dma[block], PCM_BLOCK_FRAMES * 2u, false);
}

static void __isr audio_dma_handler(void) {
    const uint32_t masks[2] = {1u << (uint)audio_dma[0], 1u << (uint)audio_dma[1]};
    uint32_t pending = dma_hw->ints1 & (masks[0] | masks[1]);
    const bool both_pending = pending == (masks[0] | masks[1]);
    if (both_pending)
        ++late_dma_blocks;

    bool urgent_snapshot = both_pending;
    for (uint handled = 0u; handled < 2u && pending != 0u; ++handled) {
        uint block = expected_dma_block;
        if ((pending & masks[block]) == 0u) {
            block ^= 1u;
            ++late_dma_blocks;
            urgent_snapshot = true;
        }
        dma_hw->ints1 = masks[block];
        pending &= ~masks[block];

        last_played_info = audio_block_info[block];
        completed_frames += PCM_BLOCK_FRAMES;
        urgent_snapshot |= audio_fill_block(block);
        rearm_block(block);
        expected_dma_block = (uint8_t)(block ^ 1u);
    }

    if (!dma_channel_is_busy((uint)audio_dma[0]) && !dma_channel_is_busy((uint)audio_dma[1])) {
        ++late_dma_blocks;
        urgent_snapshot = true;
        dma_start_channel_mask(1u << (uint)audio_dma[expected_dma_block]);
    }

    const uint32_t completed_blocks = completed_frames / PCM_BLOCK_FRAMES;
    if (urgent_snapshot || completed_blocks % AUDIO_STATUS_INTERVAL_BLOCKS == 0u) {
        publish_status_snapshot();
    }
}

bool i2s_output_init(pcm_queue_t *queue) {
    if (queue == NULL || !pio_can_add_program(I2S_PIO, &i2s_tx_program)) {
        return false;
    }
    audio_sm = pio_claim_unused_sm(I2S_PIO, false);
    if (audio_sm < 0)
        return false;
    audio_program_offset = pio_add_program(I2S_PIO, &i2s_tx_program);
    audio_dma[0] = dma_claim_unused_channel(false);
    audio_dma[1] = dma_claim_unused_channel(false);
    if (audio_dma[0] < 0 || audio_dma[1] < 0) {
        if (audio_dma[0] >= 0)
            dma_channel_unclaim((uint)audio_dma[0]);
        if (audio_dma[1] >= 0)
            dma_channel_unclaim((uint)audio_dma[1]);
        pio_remove_program(I2S_PIO, &i2s_tx_program, audio_program_offset);
        pio_sm_unclaim(I2S_PIO, (uint)audio_sm);
        return false;
    }

    uint32_t divider;
    /* The helper describes a 64-instruction frame; 64-BCK I2S takes 128. */
    if (!pcm_i2s_divider(clock_get_hz(clk_sys), PCM_SAMPLE_RATE * 2u, &divider)) {
        dma_channel_unclaim((uint)audio_dma[0]);
        dma_channel_unclaim((uint)audio_dma[1]);
        pio_remove_program(I2S_PIO, &i2s_tx_program, audio_program_offset);
        pio_sm_unclaim(I2S_PIO, (uint)audio_sm);
        return false;
    }
    pio_sm_config config = i2s_tx_program_get_default_config(audio_program_offset);
    sm_config_set_out_pins(&config, I2S_PIN_DATA, 1u);
    sm_config_set_sideset_pins(&config, I2S_PIN_BCLK);
    sm_config_set_out_shift(&config, false, true, 32u);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv_int_frac8(&config, divider >> 8u, divider & 0xffu);

    pio_gpio_init(I2S_PIO, I2S_PIN_BCLK);
    pio_gpio_init(I2S_PIO, I2S_PIN_LRCLK);
    pio_gpio_init(I2S_PIO, I2S_PIN_DATA);
    pio_sm_set_consecutive_pindirs(I2S_PIO, (uint)audio_sm, I2S_PIN_BCLK, 2u, true);
    pio_sm_set_consecutive_pindirs(I2S_PIO, (uint)audio_sm, I2S_PIN_DATA, 1u, true);
    pio_sm_init(I2S_PIO, (uint)audio_sm, audio_program_offset + i2s_tx_offset_entry_point, &config);

    audio_queue = queue;
    audio_status_queue_init(&status_queue);
    ui_status = (audio_status_snapshot_t){0};
    completed_frames = 0u;
    silence_frames = 0u;
    underrun_blocks = 0u;
    late_dma_blocks = 0u;
    dropped_snapshots = 0u;
    min_buffered_frames = UINT32_MAX;
    expected_dma_block = 0u;
    atomic_store(&producer_max_render_us, 0u);
    last_filled_info = (pcm_block_info_t){0};
    last_played_info = (pcm_block_info_t){0};
    (void)audio_fill_block(0u);
    (void)audio_fill_block(1u);

    for (uint block = 0; block < 2u; ++block) {
        dma_channel_config dma_config = dma_channel_get_default_config((uint)audio_dma[block]);
        channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
        channel_config_set_read_increment(&dma_config, true);
        channel_config_set_write_increment(&dma_config, false);
        channel_config_set_dreq(&dma_config, pio_get_dreq(I2S_PIO, (uint)audio_sm, true));
        channel_config_set_chain_to(&dma_config, (uint)audio_dma[block ^ 1u]);
        channel_config_set_high_priority(&dma_config, true);
        dma_channel_configure((uint)audio_dma[block], &dma_config, &I2S_PIO->txf[audio_sm],
                              audio_blocks[block], PCM_BLOCK_FRAMES * 2u, false);
        dma_channel_set_irq1_enabled((uint)audio_dma[block], true);
    }
    irq_add_shared_handler(DMA_IRQ_1, audio_dma_handler,
                           PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_priority(DMA_IRQ_1, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);
    pio_sm_set_enabled(I2S_PIO, (uint)audio_sm, true);
    dma_start_channel_mask(1u << (uint)audio_dma[0]);
    return true;
}

i2s_output_stats_t i2s_output_get_stats(void) {
    (void)audio_status_queue_pop_latest(&status_queue, &ui_status);
    return ui_status;
}

void i2s_output_report_render_time(uint32_t elapsed_us) {
    uint32_t observed = atomic_load_explicit(&producer_max_render_us, memory_order_relaxed);
    while (elapsed_us > observed &&
           !atomic_compare_exchange_weak_explicit(&producer_max_render_us, &observed, elapsed_us,
                                                  memory_order_relaxed, memory_order_relaxed)) {
    }
}
