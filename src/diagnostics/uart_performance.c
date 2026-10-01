#include "uart_performance.h"
#include <stdio.h>
#include <string.h>
#include "audio/pcm_format.h"

static void clean_text(char *destination, size_t capacity, const char *source) {
    size_t i = 0u;
    if (source == NULL) source = "none";
    while (i + 1u < capacity && source[i] != '\0') {
        const unsigned char byte = (unsigned char)source[i];
        destination[i] = byte < 32u || byte == 127u ? '?' : source[i];
        ++i;
    }
    destination[i] = '\0';
}

void uart_performance_init(uart_performance_t *log, const uart_performance_sample_t *initial) {
    memset(log, 0, sizeof(*log));
    log->previous = *initial;
}

bool uart_performance_report(uart_performance_t *log, const uart_performance_sample_t *sample) {
    if (log->sent < log->length) {
        ++log->dropped_reports;
        return false;
    }
    if (sample->uptime_ms <= log->previous.uptime_ms) return false;
    const uint64_t interval_ms = sample->uptime_ms - log->previous.uptime_ms;
    const uint32_t frame_delta = sample->audio.completed_frames - log->previous.audio.completed_frames;
    const uint32_t map_delta = sample->map_transfers - log->previous.map_transfers;
    const uint32_t transfer_delta = sample->display_transfers - log->previous.display_transfers;
    const uint32_t frame_rate = (uint32_t)((uint64_t)frame_delta * 1000u / interval_ms);
    const uint32_t map_rate_x10 = (uint32_t)((uint64_t)map_delta * 10000u / interval_ms);
    char source[128];
    char error[96];
    clean_text(source, sizeof(source), sample->source);
    clean_text(error, sizeof(error), sample->storage_error);

    int length = snprintf(log->text, sizeof(log->text),
        "PERF t=%llums dt=%llums gen=%lu screen=%s aram=%s requested=%uHz window_view=%s\r\n"
        " AUDIO ready=%u state=%s track=%llums rate=%lu frames/s volume=%u/8 peaks=%u/%u\r\n"
        " AUDIO silence=%lu(+%lu) underrun=%lu(+%lu) late_dma=%lu(+%lu) "
        "a_drop=%lu(+%lu) v_drop=%lu(+%lu)\r\n"
        " TIMING render_max=%luus buf_min=%lu frames (since audio start); "
        "gui_max=%luus gui_window_max=%luus\r\n"
        " DISPLAY transfers=%lu(+%lu) maps=%lu(+%lu) map_tx=%lu.%luHz "
        "last=%luus max=%luus errors=%lu(+%lu)\r\n"
        " MAP due=%lu(+%lu) pub=%lu(+%lu) busy=%lu(+%lu) headroom=%lu(+%lu) "
        "consume=%lu(+%lu) discard=%lu(+%lu); activity_window_max=%luus scale_window_max=%luus\r\n"
        " STORAGE state=%s file=%s error=%s; log_drop=%lu format_max=%luus\r\n",
        (unsigned long long)sample->uptime_ms, (unsigned long long)interval_ms,
        (unsigned long)sample->audio.generation, sample->screen == NULL ? "unknown" : sample->screen,
        sample->data_mode ? "Data" : "Activity", sample->requested_hz,
        sample->mixed_view_window ? "mixed" : "stable",
        sample->audio_ready, sample->player_state == NULL ? "unknown" : sample->player_state,
        (unsigned long long)(sample->audio.track_frames / (PCM_SAMPLE_RATE / 1000u)),
        (unsigned long)frame_rate, sample->audio.volume_step,
        sample->audio.peak_left, sample->audio.peak_right,
        (unsigned long)sample->audio.silence_frames,
        (unsigned long)(sample->audio.silence_frames - log->previous.audio.silence_frames),
        (unsigned long)sample->audio.underrun_blocks,
        (unsigned long)(sample->audio.underrun_blocks - log->previous.audio.underrun_blocks),
        (unsigned long)sample->audio.late_dma_blocks,
        (unsigned long)(sample->audio.late_dma_blocks - log->previous.audio.late_dma_blocks),
        (unsigned long)sample->audio.dropped_snapshots,
        (unsigned long)(sample->audio.dropped_snapshots - log->previous.audio.dropped_snapshots),
        (unsigned long)sample->visual_drops,
        (unsigned long)(sample->visual_drops - log->previous.visual_drops),
        (unsigned long)sample->audio.max_render_us, (unsigned long)sample->audio.min_buffered_frames,
        (unsigned long)sample->gui_max_us, (unsigned long)sample->gui_window_max_us,
        (unsigned long)sample->display_transfers, (unsigned long)transfer_delta,
        (unsigned long)sample->map_transfers, (unsigned long)map_delta,
        (unsigned long)(map_rate_x10 / 10u), (unsigned long)(map_rate_x10 % 10u),
        (unsigned long)sample->display_last_us, (unsigned long)sample->display_max_us,
        (unsigned long)sample->display_errors,
        (unsigned long)(sample->display_errors - log->previous.display_errors),
        (unsigned long)sample->maps.scheduled,
        (unsigned long)(sample->maps.scheduled - log->previous.maps.scheduled),
        (unsigned long)sample->maps.published,
        (unsigned long)(sample->maps.published - log->previous.maps.published),
        (unsigned long)sample->maps.busy,
        (unsigned long)(sample->maps.busy - log->previous.maps.busy),
        (unsigned long)sample->maps.headroom,
        (unsigned long)(sample->maps.headroom - log->previous.maps.headroom),
        (unsigned long)sample->maps_consumed,
        (unsigned long)(sample->maps_consumed - log->previous.maps_consumed),
        (unsigned long)sample->maps_discarded,
        (unsigned long)(sample->maps_discarded - log->previous.maps_discarded),
        (unsigned long)sample->activity_window_max_us, (unsigned long)sample->scale_window_max_us,
        sample->storage_state == NULL ? "unknown" : sample->storage_state, source, error,
        (unsigned long)log->dropped_reports, (unsigned long)sample->log_format_max_us);
    if (length < 0 || (size_t)length >= sizeof(log->text)) {
        ++log->dropped_reports;
        return false;
    }
    const spc_snapshot_t *snapshot = sample->snapshot;
    if (snapshot != NULL && snapshot->generation == sample->audio.generation &&
        (snapshot->validity & SPC_SNAPSHOT_VALID_DSP_REGISTERS) != 0u) {
        const uint8_t *dsp = snapshot->dsp_registers;
        const unsigned echo_delay = dsp[0x7d] & 0x0fu;
        const int echo_length = snprintf(log->text + length, sizeof(log->text) - (size_t)length,
            " ECHO copied gen=%lu EON=%02X FLG=%02X write_disabled=%u ESA=%04X "
            "EDL=%u size=%u EFB=%d EVOL=%d/%d\r\n",
            (unsigned long)snapshot->generation, dsp[0x4d], dsp[0x6c],
            (dsp[0x6c] & 0x20u) != 0u, (unsigned)dsp[0x6d] << 8u,
            echo_delay, echo_delay == 0u ? 4u : echo_delay * 2048u,
            (int)(int8_t)dsp[0x0d], (int)(int8_t)dsp[0x2c], (int)(int8_t)dsp[0x3c]);
        if (echo_length < 0 || (size_t)echo_length >= sizeof(log->text) - (size_t)length) {
            ++log->dropped_reports;
            return false;
        }
        length += echo_length;
    } else {
        const int echo_length = snprintf(log->text + length, sizeof(log->text) - (size_t)length,
                                         " ECHO unavailable (no DSP snapshot for this audio generation)\r\n");
        if (echo_length < 0 || (size_t)echo_length >= sizeof(log->text) - (size_t)length) {
            ++log->dropped_reports;
            return false;
        }
        length += echo_length;
    }
    log->length = (size_t)length;
    log->sent = 0u;
    log->previous = *sample;
    return true;
}

size_t uart_performance_poll(uart_performance_t *log, uart_performance_try_write_t try_write,
                             void *context, size_t byte_budget) {
    size_t sent = 0u;
    while (sent < byte_budget && log->sent < log->length) {
        if (!try_write(context, log->text[log->sent])) break;
        ++log->sent;
        ++sent;
    }
    return sent;
}
