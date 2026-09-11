#ifndef AUDIO_I2S_OUTPUT_H
#define AUDIO_I2S_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "pcm_queue.h"
#include "audio_status_queue.h"

typedef audio_status_snapshot_t i2s_output_stats_t;

bool i2s_output_init(pcm_queue_t *queue);
i2s_output_stats_t i2s_output_get_stats(void);
void i2s_output_report_render_time(uint32_t elapsed_us);

#endif
