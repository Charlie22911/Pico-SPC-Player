#ifndef PLAYER_COMMANDS_H
#define PLAYER_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#define PLAYER_COMMAND_CAPACITY 8u

typedef enum {
    PLAYER_IDLE = 0,
    PLAYER_PLAYING,
    PLAYER_PAUSED,
    PLAYER_FADING_FOR_PAUSE,
    PLAYER_FADING_FOR_RESTART,
    PLAYER_FADING_FOR_LOAD,
    PLAYER_LOADING,
    PLAYER_FADING_IN,
    PLAYER_ERROR,
} player_mode_t;

typedef enum {
    PLAYER_COMMAND_SET_PAUSED = 1,
    PLAYER_COMMAND_RESTART,
    PLAYER_COMMAND_SET_VOLUME,
    PLAYER_COMMAND_LOAD_EMBEDDED,
} player_command_type_t;

typedef struct {
    player_command_type_t type;
    uint8_t value;
} player_command_t;

/* Core 1 produces commands and Core 0 consumes at most one queue capacity per
 * PCM block. Push/pop are nonblocking and return false when no progress can be made. */
typedef struct {
    _Atomic uint32_t head;
    _Atomic uint32_t tail;
    player_command_t entries[PLAYER_COMMAND_CAPACITY];
} player_command_queue_t;

void player_command_queue_init(player_command_queue_t *queue);
bool player_command_push(player_command_queue_t *queue, player_command_t command);
bool player_command_pop(player_command_queue_t *queue, player_command_t *command);

#endif
