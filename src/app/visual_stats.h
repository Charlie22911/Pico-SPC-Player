#ifndef APP_VISUAL_STATS_H
#define APP_VISUAL_STATS_H
#include <stdint.h>

typedef struct {
    uint32_t scheduled;
    uint32_t published;
    uint32_t busy;
    uint32_t headroom;
} visual_pipeline_stats_t;
#endif
