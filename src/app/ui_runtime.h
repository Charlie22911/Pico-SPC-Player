#ifndef APP_UI_RUNTIME_H
#define APP_UI_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "app/visual_pipeline.h"
#include "player/player.h"
#include "player/player_commands.h"
#include "spc/spc_metadata.h"
#include "storage/storage.h"

/* Every referenced object and string must have static lifetime. Core 1 owns
 * display, touch, and storage work after ui_runtime_run() begins. */
typedef struct {
    storage_t *storage;
    player_command_queue_t *player_commands;
    const player_shared_status_t *player_status;
    const _Atomic bool *audio_ready;
    visual_pipeline_t *visuals;
    const spc_metadata_t *embedded_metadata;
    size_t embedded_size;
    const char *embedded_basename;
} ui_runtime_config_t;

/* Core 1 entry point. It owns its event loop, may sleep when idle, and never returns. */
_Noreturn void ui_runtime_run(const ui_runtime_config_t *config);

#endif
