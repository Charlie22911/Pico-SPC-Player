#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "diagnostics/uart_performance.h"

typedef struct {
    char bytes[UART_PERFORMANCE_CAPACITY * 2u];
    size_t count;
    size_t allowance;
} sink_t;

static bool try_write(void *context, char byte) {
    sink_t *sink = context;
    if (sink->allowance == 0u) return false;
    --sink->allowance;
    sink->bytes[sink->count++] = byte;
    sink->bytes[sink->count] = '\0';
    return true;
}

static void drain(uart_performance_t *log, sink_t *sink) {
    sink->allowance = sizeof(sink->bytes) - 1u - sink->count;
    while (uart_performance_poll(log, try_write, sink, 8u) != 0u) {}
}

int main(void) {
    uart_performance_t log;
    uart_performance_sample_t initial = {0};
    initial.audio.silence_frames = UINT32_MAX - 255u;
    initial.audio.underrun_blocks = 1u;
    uart_performance_init(&log, &initial);
    spc_snapshot_t dsp = {0};
    dsp.generation = 7u;
    dsp.validity = SPC_SNAPSHOT_VALID_DSP_REGISTERS;
    dsp.dsp_registers[0x4d] = 0x80u;
    dsp.dsp_registers[0x6d] = 0x60u;
    dsp.dsp_registers[0x7d] = 3u;
    dsp.dsp_registers[0x0d] = 0xffu;
    uart_performance_sample_t sample = {0};
    sample.uptime_ms = 5000u;
    sample.audio.completed_frames = 160000u;
    sample.audio.silence_frames = 0u; /* Exactly 256 new frames across rollover. */
    sample.audio.underrun_blocks = 2u;
    sample.audio.generation = 7u;
    sample.audio.track_frames = 96000u;
    sample.audio.max_render_us = 5400u;
    sample.audio.min_buffered_frames = 1536u;
    sample.audio_ready = true;
    sample.map_transfers = 300u;
    sample.display_transfers = 600u;
    sample.requested_hz = 60u;
    sample.data_mode = true;
    sample.screen = "home";
    sample.player_state = "playing";
    sample.storage_state = "ready";
    sample.source = "SNEStronizer.spc\r\nFAKE REPORT";
    sample.snapshot = &dsp;
    sample.maps = (visual_pipeline_stats_t){300u, 301u, 2u, 3u};
    sample.maps_consumed = 299u;
    sample.maps_discarded = 2u;
    sample.activity_window_max_us = 1234u;
    sample.scale_window_max_us = 5678u;
    assert(uart_performance_report(&log, &sample));

    /* Busy UART must return immediately and retain every pending byte. */
    sink_t sink = {0};
    assert(uart_performance_poll(&log, try_write, &sink, 8u) == 0u);
    assert(log.sent == 0u);
    sink.allowance = 100u;
    assert(uart_performance_poll(&log, try_write, &sink, 0u) == 0u);
    assert(uart_performance_poll(&log, try_write, &sink, 8u) == 8u);
    sink.allowance = 3u;
    assert(uart_performance_poll(&log, try_write, &sink, 8u) == 3u);
    const size_t pending_sent = log.sent;
    assert(!uart_performance_report(&log, &sample));
    assert(log.sent == pending_sent && log.dropped_reports == 1u);
    drain(&log, &sink);
    assert(strstr(sink.bytes, "silence=0(+256)") != NULL);
    assert(strstr(sink.bytes, "underrun=2(+1)") != NULL);
    assert(strstr(sink.bytes, "rate=32000 frames/s") != NULL);
    assert(strstr(sink.bytes, "map_tx=60.0Hz") != NULL);
    assert(strstr(sink.bytes, "due=300(+300) pub=301(+301) busy=2(+2) headroom=3(+3)") != NULL);
    assert(strstr(sink.bytes, "consume=299(+299) discard=2(+2)") != NULL);
    assert(strstr(sink.bytes, "activity_window_max=1234us scale_window_max=5678us") != NULL);
    assert(strstr(sink.bytes, "EON=80") != NULL);
    assert(strstr(sink.bytes, "ESA=6000") != NULL);
    assert(strstr(sink.bytes, "size=6144") != NULL);
    assert(strstr(sink.bytes, "EFB=-1") != NULL);
    assert(strstr(sink.bytes, "SNEStronizer.spc??FAKE REPORT") != NULL);
    assert(strstr(sink.bytes, "\r\nFAKE REPORT") == NULL);

    /* No stale DSP data or stale rate after track/view changes and an idle window. */
    sample.uptime_ms = 10000u;
    sample.audio.generation = 8u;
    sample.mixed_view_window = true;
    assert(uart_performance_report(&log, &sample));
    sink = (sink_t){0};
    drain(&log, &sink);
    assert(strstr(sink.bytes, "map_tx=0.0Hz") != NULL);
    assert(strstr(sink.bytes, "window_view=mixed") != NULL);
    assert(strstr(sink.bytes, "ECHO unavailable") != NULL);
    assert(strstr(sink.bytes, "log_drop=1") != NULL);
    /* Equal timestamps cannot cause division by zero or queue nonsense. */
    assert(!uart_performance_report(&log, &sample));
    /* Long names and large counters still produce a complete bounded report. */
    char long_text[256];
    memset(long_text, 'X', sizeof(long_text) - 1u);
    long_text[sizeof(long_text) - 1u] = '\0';
    sample.uptime_ms = 15000u;
    sample.source = long_text;
    sample.storage_error = long_text;
    sample.audio.completed_frames = UINT32_MAX;
    sample.audio.track_frames = UINT64_MAX;
    sample.audio.silence_frames = UINT32_MAX;
    sample.audio.underrun_blocks = UINT32_MAX;
    sample.audio.dropped_snapshots = UINT32_MAX;
    sample.audio.late_dma_blocks = UINT32_MAX;
    sample.audio.max_render_us = UINT32_MAX;
    sample.audio.min_buffered_frames = UINT32_MAX;
    sample.visual_drops = UINT32_MAX;
    sample.display_transfers = UINT32_MAX;
    sample.map_transfers = UINT32_MAX;
    sample.display_errors = UINT32_MAX;
    sample.gui_max_us = UINT32_MAX;
    sample.gui_window_max_us = UINT32_MAX;
    sample.display_last_us = UINT32_MAX;
    sample.display_max_us = UINT32_MAX;
    sample.log_format_max_us = UINT32_MAX;
    sample.maps = (visual_pipeline_stats_t){UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    sample.maps_consumed = UINT32_MAX;
    sample.maps_discarded = UINT32_MAX;
    sample.activity_window_max_us = UINT32_MAX;
    sample.scale_window_max_us = UINT32_MAX;
    sample.audio.generation = 7u;
    dsp.dsp_registers[0x7d] = 0u;
    assert(uart_performance_report(&log, &sample));
    sink = (sink_t){0};
    drain(&log, &sink);
    assert(strstr(sink.bytes, "size=4") != NULL);
    assert(strstr(sink.bytes, "track=576460752303423487ms") != NULL);
    assert(sink.count < UART_PERFORMANCE_CAPACITY);
    assert(sink.count >= 2u && strcmp(sink.bytes + sink.count - 2u, "\r\n") == 0);
    puts("UART performance reporting passed");
    return 0;
}
