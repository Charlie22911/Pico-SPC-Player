#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "ff.h"
#include "spc/spc_metadata.h"
#include "spc_catalog.h"
#include "storage_job.h"
#include "storage_mailbox.h"

typedef enum {
    STORAGE_STATE_BOOTING = 0,
    STORAGE_STATE_SCANNING,
    STORAGE_STATE_NO_CARD,
    STORAGE_STATE_READY,
    STORAGE_STATE_LOADING,
    STORAGE_STATE_EMPTY,
    STORAGE_STATE_CARD_ERROR,
    STORAGE_STATE_READ_ERROR,
} storage_state_t;

typedef struct {
    FATFS filesystem;
    spc_library_catalog_t catalog;
    storage_mailbox_t mailbox;
    storage_job_t job;
    spc_metadata_t active_metadata;
    spc_metadata_t pending_metadata;
    storage_state_t state;
    const char *error;
    char current_folder[SPC_LFN_CAPACITY];
    char current_filename[SPC_LFN_CAPACITY];
    char pending_folder[SPC_LFN_CAPACITY];
    char pending_filename[SPC_LFN_CAPACITY];
    spc_track_ref_t requested_track;
    spc_track_ref_t visible_requests[3];
    visible_title_t visible_result;
    uint32_t visible_page_revision;
    uint32_t current_file_size;
    uint32_t pending_file_size;
    uint32_t pending_generation;
    bool card_detected_at_boot;
    bool mounted;
    bool has_current_track;
    bool request_pending;
    bool request_is_restart;
    bool activation_pending;
    uint8_t visible_request_count;
    uint8_t visible_request_index;
    bool visible_request_pending;
    bool visible_result_ready;
} storage_t;

void storage_init(storage_t *storage);
void storage_begin(storage_t *storage);
bool storage_open_folder(storage_t *storage, uint16_t folder_index);
bool storage_request_track(storage_t *storage, spc_track_ref_t track);
bool storage_request_restart(storage_t *storage);
bool storage_request_visible_titles(storage_t *storage, uint32_t page_revision,
                                    const spc_track_ref_t *tracks, uint8_t count);
bool storage_take_visible_title(storage_t *storage, visible_title_t *result);
void storage_observe_played_generation(storage_t *storage, uint32_t generation);
void storage_poll(storage_t *storage);

#endif
