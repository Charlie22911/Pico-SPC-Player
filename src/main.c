#include <stdatomic.h>
#include <stdio.h>

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "app/ui_runtime.h"
#include "app/visual_pipeline.h"
#include "audio/i2s_output.h"
#include "audio/pcm_queue.h"
#include "board/board.h"
#include "player/player.h"
#include "spc/embedded_spc.h"
#include "spc/spc_metadata.h"
#include "storage/storage.h"

/* Core 0 owns player/backend mutation and PCM production. Core 1 owns FatFs,
 * touch, UI drawing, and display DMA. Cross-core work uses bounded queues,
 * atomic requests, and explicit mailboxes; visualization never delays audio. */
static pcm_queue_t pcm_queue;
static _Atomic bool audio_ready;

static player_command_queue_t player_commands;
static player_shared_status_t player_shared;
static player_t player;

static storage_t storage_manager;
static visual_pipeline_t visual_pipeline;

static spc_metadata_t embedded_metadata;
static ui_runtime_config_t ui_runtime;

static void core1_entry(void) {
    ui_runtime_run(&ui_runtime);
}

static void print_memory_diagnostics(void) {
    extern char __end__, __HeapLimit;
    extern void *_sbrk(int increment);
    const uintptr_t heap_start = (uintptr_t)&__end__;
    const uintptr_t heap_limit = (uintptr_t)&__HeapLimit;
    const uintptr_t heap_break = (uintptr_t)_sbrk(0);
    printf("MEMORY static_main_SRAM=%lu heap_region=%lu heap_unused_tail=%lu bytes "
           "(tail excludes free blocks within allocated heap)\n",
           (unsigned long)(heap_start - 0x20000000u), (unsigned long)(heap_limit - heap_start),
           (unsigned long)(heap_break >= heap_start && heap_break <= heap_limit ? heap_limit - heap_break : 0u));
    printf("BOOT watchdog_reboot=%u; STACK core0=%u core1=%u bytes\n",
           watchdog_caused_reboot(), PICO_STACK_SIZE, PICO_CORE1_STACK_SIZE);
    printf("AUDIO sample_rate=%uHz stereo16 slots32 block=%u frames budget=%uus queue=%u frames\n",
           PCM_SAMPLE_RATE, PCM_BLOCK_FRAMES, PCM_BLOCK_FRAMES * 1000000u / PCM_SAMPLE_RATE,
           PCM_QUEUE_CAPACITY);
}

int main(void) {
    const board_result_t board_result = board_init();
    const board_status_t board = board_get_status();
    board_print_diagnostics();
    printf("Clock sys=%lu peri=%lu PSRAM(optional)=%u result=%d\n",
           (unsigned long)board.sys_clock_hz, (unsigned long)board.peri_clock_hz,
           (unsigned)board.psram_capacity_bytes, board_result);
    if (!board.clock_ready) {
        printf("Clock verification failed; playback stopped. Reset after correcting the clock configuration.\n");
        while (true) {
            tight_loop_contents();
        }
    }

    storage_init(&storage_manager);
    atomic_init(&audio_ready, false);
    player_command_queue_init(&player_commands);
    visual_pipeline_init(&visual_pipeline, 60u);

    const size_t embedded_size = embedded_spc_size();
#if PICO_SPC_HAS_EMBEDDED
    const bool metadata_ok =
        spc_metadata_parse(embedded_spc_start, embedded_size, &embedded_metadata);
    printf("Embedded SPC metadata=%s title=%s\n", metadata_ok ? "OK" : "invalid",
           embedded_metadata.title[0] != '\0' ? embedded_metadata.title : "unknown");
#else
    printf("Embedded SPC disabled\n");
#endif

    const char *error = player_init_idle(&player, embedded_spc_start, embedded_size,
                                         &player_commands, &player_shared);
    player_bind_storage_mailbox(&player, &storage_manager.mailbox);
    visual_pipeline_bind_backend(&visual_pipeline, player.backend);
    printf("Player idle: embedded bytes=%u init=%s\n", (unsigned)embedded_size,
           error == NULL ? "OK" : error);
    printf("Software SPC backend=%u bytes\n", (unsigned)software_spc_backend_size());
    print_memory_diagnostics();

    ui_runtime = (ui_runtime_config_t){
        .storage = &storage_manager,
        .player_commands = &player_commands,
        .player_status = &player_shared,
        .audio_ready = &audio_ready,
        .visuals = &visual_pipeline,
        .embedded_metadata = embedded_size != 0u ? &embedded_metadata : NULL,
        .embedded_size = embedded_size,
        .embedded_basename = EMBEDDED_SPC_BASENAME,
    };

    pcm_queue_init(&pcm_queue);
    uint32_t block[PCM_BLOCK_FRAMES];
    pcm_block_info_t block_info;
    while (error == NULL && pcm_queue_space(&pcm_queue) >= PCM_BLOCK_FRAMES) {
        if (!player_render_block(&player, block, &block_info)) {
            break;
        }
        if (pcm_queue_write_block(&pcm_queue, block, &block_info)) {
            visual_pipeline_service(&visual_pipeline, &player, &pcm_queue, true);
        }
    }

    const bool started = i2s_output_init(&pcm_queue);
    atomic_store_explicit(&audio_ready, started, memory_order_release);
    printf("I2S %s: BCLK GP26, LRCLK GP27, DATA GP28\n", started ? "started" : "failed");
    multicore_launch_core1(core1_entry);

    while (true) {
        if (pcm_queue_space(&pcm_queue) >= PCM_BLOCK_FRAMES) {
            const uint32_t render_started_us = time_us_32();
            (void)player_render_block(&player, block, &block_info);
            i2s_output_report_render_time(time_us_32() - render_started_us);
            if (pcm_queue_write_block(&pcm_queue, block, &block_info)) {
                visual_pipeline_service(&visual_pipeline, &player, &pcm_queue, true);
            }
        } else {
            /* Forced requests are serviced here only when the audio queue has headroom. */
            visual_pipeline_service(&visual_pipeline, &player, &pcm_queue, false);
            tight_loop_contents();
        }
    }
}
