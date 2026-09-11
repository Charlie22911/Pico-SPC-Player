#include "storage.h"

#include <string.h>

#include "sd_card.h"

#define STORAGE_LOAD_CHUNK_BYTES 1024u

_Static_assert(SPC_LFN_CAPACITY == FF_MAX_LFN + 1u, "catalog names must match FatFs LFN capacity");

static bool ignored_name(const char *name) {
    return name == NULL || name[0] == '\0' || name[0] == '.';
}

static bool visible_job_state(storage_job_state_t state) {
    return state == STORAGE_JOB_VISIBLE_TITLE_OPEN || state == STORAGE_JOB_VISIBLE_TITLE_READ ||
           state == STORAGE_JOB_VISIBLE_TITLE_PUBLISH;
}

static void cancel_visible_job(storage_t *storage) {
    if (!visible_job_state(storage->job.state))
        return;
    storage_job_close(&storage->job);
    storage->job.state = STORAGE_JOB_IDLE;
    /* A playback load preempts metadata; resume this row afterwards. */
}

static void set_error(storage_t *storage, storage_state_t state, const char *message,
                      FRESULT result) {
    storage_job_close(&storage->job);
    if (atomic_load_explicit(&storage->mailbox.state, memory_order_acquire) ==
        STORAGE_MAILBOX_WRITING) {
        storage_mailbox_cancel_write(&storage->mailbox);
    }
    storage->job.state = STORAGE_JOB_IDLE;
    storage->job.result = result;
    storage->state = state;
    storage->error = message;
}

static bool make_path(char *path, const char *folder, const char *filename) {
    const size_t folder_length = strlen(folder);
    const size_t filename_length = filename == NULL ? 0u : strlen(filename);
    const size_t length = 1u + folder_length + (filename == NULL ? 0u : 1u + filename_length);
    if (length + 1u > SPC_PATH_CAPACITY)
        return false;
    path[0] = '/';
    memcpy(path + 1u, folder, folder_length);
    if (filename != NULL) {
        path[1u + folder_length] = '/';
        memcpy(path + 2u + folder_length, filename, filename_length);
    }
    path[length] = '\0';
    return true;
}

static void finish_root_scan(storage_t *storage) {
    storage_job_close(&storage->job);
    spc_library_sort_folders(&storage->catalog);
    storage->job.state = STORAGE_JOB_IDLE;
    storage->state =
        storage->catalog.folder_count == 0u ? STORAGE_STATE_EMPTY : STORAGE_STATE_READY;
    storage->error = storage->catalog.folder_overflow      ? "FOLDER LIMIT REACHED"
                     : storage->catalog.folder_count == 0u ? "NO GAME FOLDERS"
                                                           : NULL;
}

static void finish_folder_scan(storage_t *storage) {
    storage_job_close(&storage->job);
    spc_library_sort_tracks(&storage->catalog);
    storage->job.state = STORAGE_JOB_IDLE;
    storage->state = storage->catalog.track_count == 0u ? STORAGE_STATE_EMPTY : STORAGE_STATE_READY;
    storage->error = storage->catalog.track_overflow      ? "TRACK LIMIT REACHED"
                     : storage->catalog.track_count == 0u ? "NO SPC FILES"
                                                          : NULL;
}

void storage_init(storage_t *storage) {
    if (storage == NULL)
        return;
    memset(storage, 0, sizeof(*storage));
    spc_library_init(&storage->catalog);
    storage_mailbox_init(&storage->mailbox);
    storage_job_init(&storage->job);
    storage->state = STORAGE_STATE_BOOTING;
}

void storage_begin(storage_t *storage) {
    if (storage == NULL)
        return;
    const FRESULT result = f_mount(&storage->filesystem, "", 1u);
    storage->card_detected_at_boot = sd_card_present();
    if (result != FR_OK) {
        storage->state =
            storage->card_detected_at_boot ? STORAGE_STATE_CARD_ERROR : STORAGE_STATE_NO_CARD;
        storage->error = storage->card_detected_at_boot ? "CARD MOUNT FAILED" : NULL;
        return;
    }
    storage->mounted = true;
    storage->state = STORAGE_STATE_SCANNING;
    storage->job.state = STORAGE_JOB_SCAN_ROOT_OPEN;
}

bool storage_open_folder(storage_t *storage, uint16_t folder_index) {
    if (storage == NULL || !storage->mounted ||
        (storage->job.state != STORAGE_JOB_IDLE && !visible_job_state(storage->job.state)) ||
        storage->activation_pending || storage->request_pending ||
        folder_index >= storage->catalog.folder_count)
        return false;
    cancel_visible_job(storage);
    storage->visible_request_pending = false;
    storage->visible_request_count = 0u;
    storage->visible_result_ready = false;
    spc_library_begin_folder(&storage->catalog, folder_index);
    storage->job.state = STORAGE_JOB_SCAN_FOLDER_OPEN;
    storage->state = STORAGE_STATE_SCANNING;
    storage->error = NULL;
    return true;
}

bool storage_request_track(storage_t *storage, spc_track_ref_t track) {
    if (storage == NULL || !storage->mounted ||
        !spc_library_track_ref_valid(&storage->catalog, track) || storage->request_pending ||
        storage->activation_pending ||
        (storage->job.state != STORAGE_JOB_IDLE && !visible_job_state(storage->job.state)))
        return false;
    cancel_visible_job(storage);
    storage->requested_track = track;
    storage->request_is_restart = false;
    storage->request_pending = true;
    return true;
}

bool storage_request_restart(storage_t *storage) {
    if (storage == NULL || !storage->mounted || !storage->has_current_track ||
        (storage->job.state != STORAGE_JOB_IDLE && !visible_job_state(storage->job.state)) ||
        storage->activation_pending || storage->request_pending)
        return false;
    cancel_visible_job(storage);
    storage->request_is_restart = true;
    storage->request_pending = true;
    return true;
}

bool storage_request_visible_titles(storage_t *storage, uint32_t page_revision,
                                    const spc_track_ref_t *tracks, uint8_t count) {
    if (storage == NULL || tracks == NULL || count == 0u || count > 3u || !storage->mounted)
        return false;
    for (uint8_t i = 0u; i < count; ++i) {
        if (!spc_library_track_ref_valid(&storage->catalog, tracks[i]))
            return false;
    }
    cancel_visible_job(storage);
    memcpy(storage->visible_requests, tracks, (size_t)count * sizeof(tracks[0]));
    storage->visible_page_revision = page_revision;
    storage->visible_request_count = count;
    storage->visible_request_index = 0u;
    storage->visible_request_pending = true;
    storage->visible_result_ready = false;
    return true;
}

bool storage_take_visible_title(storage_t *storage, visible_title_t *result) {
    if (storage == NULL || result == NULL || !storage->visible_result_ready)
        return false;
    *result = storage->visible_result;
    storage->visible_result_ready = false;
    return true;
}

void storage_observe_played_generation(storage_t *storage, uint32_t generation) {
    if (storage == NULL || !storage->activation_pending ||
        generation != storage->pending_generation)
        return;
    storage->active_metadata = storage->pending_metadata;
    (void)strcpy(storage->current_folder, storage->pending_folder);
    (void)strcpy(storage->current_filename, storage->pending_filename);
    storage->has_current_track = true;
    storage->current_file_size = storage->pending_file_size;
    storage->activation_pending = false;
    storage->state = STORAGE_STATE_READY;
    storage->error = NULL;
}

static void begin_requested_load(storage_t *storage) {
    storage->request_pending = false;
    if (storage->request_is_restart) {
        storage->request_is_restart = false;
        if (!make_path(storage->job.path, storage->current_folder, storage->current_filename)) {
            set_error(storage, STORAGE_STATE_READ_ERROR, "TRACK PATH TOO LONG", FR_INVALID_NAME);
            return;
        }
        storage->job.target = (spc_track_ref_t){0};
        storage->job.load_size = storage->current_file_size < SPC_CORE_LOAD_BYTES
                                     ? storage->current_file_size
                                     : SPC_CORE_LOAD_BYTES;
        storage->job.offset = 0u;
        (void)strcpy(storage->pending_folder, storage->current_folder);
        (void)strcpy(storage->pending_filename, storage->current_filename);
        storage->pending_file_size = storage->current_file_size;
        storage->job.state = STORAGE_JOB_LOAD_OPEN;
        storage->state = STORAGE_STATE_LOADING;
        storage->error = NULL;
        return;
    }
    const spc_track_ref_t track = storage->requested_track;
    if (!spc_library_track_ref_valid(&storage->catalog, track)) {
        set_error(storage, STORAGE_STATE_READ_ERROR, "TRACK LIST CHANGED", FR_INVALID_OBJECT);
        return;
    }
    const uint16_t folder = storage->catalog.open_folder;
    const spc_track_entry_t *entry = &storage->catalog.tracks[track.track_index];
    if (!make_path(storage->job.path, storage->catalog.folders[folder].name, entry->filename)) {
        set_error(storage, STORAGE_STATE_READ_ERROR, "TRACK PATH TOO LONG", FR_INVALID_NAME);
        return;
    }
    storage->job.target = track;
    storage->job.load_size =
        entry->file_size < SPC_CORE_LOAD_BYTES ? entry->file_size : SPC_CORE_LOAD_BYTES;
    storage->job.offset = 0u;
    (void)strcpy(storage->pending_folder, storage->catalog.folders[folder].name);
    (void)strcpy(storage->pending_filename, entry->filename);
    storage->pending_file_size = entry->file_size;
    storage->job.state = STORAGE_JOB_LOAD_OPEN;
    storage->state = STORAGE_STATE_LOADING;
    storage->error = NULL;
}

static void begin_visible_title(storage_t *storage) {
    if (storage->visible_request_index >= storage->visible_request_count) {
        storage->visible_request_pending = false;
        return;
    }
    const uint8_t row = storage->visible_request_index;
    const spc_track_ref_t track = storage->visible_requests[row];
    storage->job.target = track;
    storage->job.page_revision = storage->visible_page_revision;
    storage->job.row = row;
    storage->job.title_state = VISIBLE_TITLE_INVALID;
    storage->job.metadata = (spc_metadata_t){0};
    if (!spc_library_track_ref_valid(&storage->catalog, track) ||
        !make_path(storage->job.path, storage->catalog.folders[storage->catalog.open_folder].name,
                   storage->catalog.tracks[track.track_index].filename)) {
        storage->job.state = STORAGE_JOB_VISIBLE_TITLE_PUBLISH;
        return;
    }
    storage->job.state = STORAGE_JOB_VISIBLE_TITLE_OPEN;
}

void storage_poll(storage_t *storage) {
    if (storage == NULL)
        return;
    if (storage->job.state == STORAGE_JOB_WAIT_LOAD_ACK) {
        bool accepted = false;
        uint32_t generation = 0u;
        if (!storage_mailbox_reclaim(&storage->mailbox, &accepted, &generation))
            return;
        storage->job.state = STORAGE_JOB_IDLE;
        if (!accepted) {
            storage->state = STORAGE_STATE_READ_ERROR;
            storage->error = "SPC LOAD FAILED";
        } else {
            storage->pending_metadata = storage->job.metadata;
            storage->pending_generation = generation;
            storage->activation_pending = true;
        }
        return;
    }
    if (storage->job.state == STORAGE_JOB_IDLE && storage->request_pending) {
        begin_requested_load(storage);
        return;
    }
    if (storage->job.state == STORAGE_JOB_IDLE && storage->visible_request_pending &&
        !storage->visible_result_ready) {
        begin_visible_title(storage);
        return;
    }

    FILINFO info;
    switch (storage->job.state) {
    case STORAGE_JOB_SCAN_ROOT_OPEN: {
        const FRESULT result = f_opendir(&storage->job.directory, "/");
        if (result != FR_OK) {
            set_error(storage, STORAGE_STATE_CARD_ERROR, "CARD SCAN OPEN ERROR", result);
            return;
        }
        storage->job.directory_open = true;
        storage->job.state = STORAGE_JOB_SCAN_ROOT_ENTRY;
        break;
    }
    case STORAGE_JOB_SCAN_ROOT_ENTRY: {
        const FRESULT result = f_readdir(&storage->job.directory, &info);
        if (result != FR_OK) {
            set_error(storage, STORAGE_STATE_CARD_ERROR, "CARD SCAN READ ERROR", result);
        } else if (info.fname[0] == '\0') {
            finish_root_scan(storage);
        } else if ((info.fattrib & AM_DIR) != 0u && !ignored_name(info.fname)) {
            (void)spc_library_add_folder(&storage->catalog, info.fname,
                                         (info.fattrib & (AM_HID | AM_SYS)) != 0u);
        }
        break;
    }
    case STORAGE_JOB_SCAN_FOLDER_OPEN: {
        const uint16_t folder = storage->catalog.open_folder;
        if (folder >= storage->catalog.folder_count ||
            !make_path(storage->job.path, storage->catalog.folders[folder].name, NULL)) {
            set_error(storage, STORAGE_STATE_READ_ERROR, "FOLDER PATH ERROR", FR_INVALID_NAME);
            break;
        }
        const FRESULT result = f_opendir(&storage->job.directory, storage->job.path);
        if (result != FR_OK) {
            set_error(storage, STORAGE_STATE_READ_ERROR, "FOLDER OPEN ERROR", result);
            break;
        }
        storage->job.directory_open = true;
        storage->job.state = STORAGE_JOB_SCAN_FOLDER_ENTRY;
        break;
    }
    case STORAGE_JOB_SCAN_FOLDER_ENTRY: {
        const FRESULT result = f_readdir(&storage->job.directory, &info);
        if (result != FR_OK) {
            set_error(storage, STORAGE_STATE_READ_ERROR, "FOLDER READ ERROR", result);
        } else if (info.fname[0] == '\0') {
            finish_folder_scan(storage);
        } else if (!ignored_name(info.fname)) {
            (void)spc_library_add_track(&storage->catalog, info.fname, (uint32_t)info.fsize,
                                        (info.fattrib & (AM_HID | AM_SYS)) != 0u,
                                        (info.fattrib & AM_DIR) != 0u);
        }
        break;
    }
    case STORAGE_JOB_LOAD_OPEN: {
        if (storage_mailbox_begin_write(&storage->mailbox) == NULL)
            break;
        const FRESULT result = f_open(&storage->job.file, storage->job.path, FA_READ);
        if (result != FR_OK) {
            set_error(storage, STORAGE_STATE_READ_ERROR,
                      sd_card_ready() ? "SPC OPEN ERROR" : "CARD UNAVAILABLE", result);
            break;
        }
        storage->job.file_open = true;
        /* The card file may have changed since enumeration or restart. */
        const FSIZE_t file_size = f_size(&storage->job.file);
        if (file_size < SPC_CORE_MIN_BYTES) {
            set_error(storage, STORAGE_STATE_READ_ERROR, "INVALID SPC FILE", FR_INVALID_OBJECT);
            break;
        }
        storage->pending_file_size = (uint32_t)file_size;
        storage->job.load_size =
            file_size < SPC_CORE_LOAD_BYTES ? (size_t)file_size : SPC_CORE_LOAD_BYTES;
        storage->job.offset = 0u;
        storage->job.state = STORAGE_JOB_LOAD_CHUNK;
        break;
    }
    case STORAGE_JOB_LOAD_CHUNK: {
        const size_t remaining = storage->job.load_size - storage->job.offset;
        const UINT requested =
            (UINT)(remaining < STORAGE_LOAD_CHUNK_BYTES ? remaining : STORAGE_LOAD_CHUNK_BYTES);
        UINT received = 0u;
        const FRESULT result = f_read(
            &storage->job.file, storage->mailbox.image + storage->job.offset, requested, &received);
        storage->job.offset += received;
        if (result != FR_OK || (received == 0u && remaining != 0u)) {
            set_error(storage, STORAGE_STATE_READ_ERROR,
                      sd_card_ready() ? "CARD READ ERROR" : "CARD UNAVAILABLE", result);
        } else if (storage->job.offset >= storage->job.load_size) {
            const FRESULT close_result = f_close(&storage->job.file);
            storage->job.file_open = false;
            if (close_result != FR_OK) {
                set_error(storage, STORAGE_STATE_READ_ERROR, "CARD CLOSE ERROR", close_result);
            } else {
                storage->job.state = STORAGE_JOB_LOAD_VALIDATE;
            }
        }
        break;
    }
    case STORAGE_JOB_LOAD_VALIDATE:
        if (storage->job.load_size < SPC_CORE_MIN_BYTES ||
            !spc_metadata_parse(storage->mailbox.image, storage->job.load_size,
                                &storage->job.metadata)) {
            set_error(storage, STORAGE_STATE_READ_ERROR, "INVALID SPC FILE", FR_INVALID_OBJECT);
        } else {
            storage->job.state = STORAGE_JOB_LOAD_PUBLISH;
        }
        break;
    case STORAGE_JOB_LOAD_PUBLISH:
        storage_mailbox_publish(&storage->mailbox, storage->job.load_size, storage->job.target);
        storage->job.state = STORAGE_JOB_WAIT_LOAD_ACK;
        break;
    case STORAGE_JOB_VISIBLE_TITLE_OPEN: {
        const FRESULT result = f_open(&storage->job.file, storage->job.path, FA_READ);
        if (result != FR_OK) {
            storage->job.result = result;
            storage->job.title_state = VISIBLE_TITLE_INVALID;
            storage->job.state = STORAGE_JOB_VISIBLE_TITLE_PUBLISH;
        } else {
            storage->job.file_open = true;
            storage->job.state = STORAGE_JOB_VISIBLE_TITLE_READ;
        }
        break;
    }
    case STORAGE_JOB_VISIBLE_TITLE_READ: {
        UINT received = 0u;
        const FRESULT result = f_read(&storage->job.file, storage->job.metadata_header,
                                      (UINT)sizeof(storage->job.metadata_header), &received);
        const FRESULT close_result = f_close(&storage->job.file);
        storage->job.file_open = false;
        storage->job.result = result != FR_OK ? result : close_result;
        if (result == FR_OK && close_result == FR_OK &&
            received == sizeof(storage->job.metadata_header) &&
            spc_metadata_parse(storage->job.metadata_header, received, &storage->job.metadata)) {
            storage->job.title_state = storage->job.metadata.title[0] == '\0'
                                           ? VISIBLE_TITLE_FILENAME
                                           : VISIBLE_TITLE_ID666;
        } else {
            storage->job.title_state = VISIBLE_TITLE_INVALID;
        }
        storage->job.state = STORAGE_JOB_VISIBLE_TITLE_PUBLISH;
        break;
    }
    case STORAGE_JOB_VISIBLE_TITLE_PUBLISH:
        if (storage->visible_result_ready)
            break;
        storage->visible_result = (visible_title_t){
            .track = storage->job.target,
            .page_revision = storage->job.page_revision,
            .row = storage->job.row,
            .state = storage->job.title_state,
        };
        if (storage->job.title_state == VISIBLE_TITLE_ID666) {
            (void)strcpy(storage->visible_result.title, storage->job.metadata.title);
        }
        storage->visible_result_ready = true;
        ++storage->visible_request_index;
        if (storage->visible_request_index >= storage->visible_request_count) {
            storage->visible_request_pending = false;
        }
        storage->job.state = STORAGE_JOB_IDLE;
        break;
    case STORAGE_JOB_IDLE:
    case STORAGE_JOB_WAIT_LOAD_ACK:
    default:
        break;
    }
}
