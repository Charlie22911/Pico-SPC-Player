#ifndef DIAGNOSTICS_UART_PERFORMANCE_H
#define DIAGNOSTICS_UART_PERFORMANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio/audio_status_queue.h"
#include "spc/spc_snapshot.h"
#include "app/visual_stats.h"

#define UART_PERFORMANCE_CAPACITY 2048u

typedef struct {
    uint64_t uptime_ms;
    audio_status_snapshot_t audio;
    uint32_t visual_drops;
    uint32_t display_transfers;
    uint32_t map_transfers;
    uint32_t display_errors;
    uint32_t gui_max_us;
    uint32_t gui_window_max_us;
    uint32_t display_last_us;
    uint32_t display_max_us;
    uint32_t log_format_max_us;
    visual_pipeline_stats_t maps;
    uint32_t maps_consumed;
    uint32_t maps_discarded;
    uint32_t activity_window_max_us;
    uint32_t scale_window_max_us;
    uint8_t requested_hz;
    bool audio_ready;
    bool data_mode;
    bool mixed_view_window;
    const char *screen;
    const char *player_state;
    const char *storage_state;
    const char *source;
    const char *storage_error;
    const spc_snapshot_t *snapshot;
} uart_performance_sample_t;

/* Core 1 owns this buffer. Never replaces an unfinished report or waits for TX. */
typedef struct {
    char text[UART_PERFORMANCE_CAPACITY];
    size_t length;
    size_t sent;
    uint32_t dropped_reports;
    uart_performance_sample_t previous;
} uart_performance_t;

typedef bool (*uart_performance_try_write_t)(void *context, char byte);

void uart_performance_init(uart_performance_t *log, const uart_performance_sample_t *initial);
bool uart_performance_report(uart_performance_t *log, const uart_performance_sample_t *sample);
/* A false callback return means FIFO full; leave the byte pending for next poll. */
size_t uart_performance_poll(uart_performance_t *log, uart_performance_try_write_t try_write,
                             void *context, size_t byte_budget);

#endif
