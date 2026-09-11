#ifndef STORAGE_SPC_CATALOG_H
#define STORAGE_SPC_CATALOG_H

#include <stdbool.h>
#include <stdint.h>

#define SPC_FOLDER_CAPACITY 96u
#define SPC_TRACK_CAPACITY 192u
#define SPC_LFN_CAPACITY 128u
#define SPC_PATH_CAPACITY (SPC_LFN_CAPACITY * 2u + 2u)
#define SPC_CORE_MIN_BYTES 0x10180u
#define SPC_CORE_LOAD_BYTES 0x10200u

typedef struct {
    char name[SPC_LFN_CAPACITY];
} spc_folder_entry_t;

typedef struct {
    char filename[SPC_LFN_CAPACITY];
    uint32_t file_size;
} spc_track_entry_t;

typedef struct {
    uint32_t folder_revision;
    uint16_t track_index;
} spc_track_ref_t;

typedef enum {
    VISIBLE_TITLE_FILENAME = 0,
    VISIBLE_TITLE_LOADING,
    VISIBLE_TITLE_ID666,
    VISIBLE_TITLE_INVALID,
} visible_title_state_t;

typedef struct {
    spc_track_ref_t track;
    uint32_t page_revision;
    uint8_t row;
    visible_title_state_t state;
    char title[33];
} visible_title_t;

typedef struct {
    spc_folder_entry_t folders[SPC_FOLDER_CAPACITY];
    spc_track_entry_t tracks[SPC_TRACK_CAPACITY];
    uint16_t folder_count;
    uint16_t track_count;
    uint16_t open_folder;
    uint32_t folder_revision;
    bool folder_overflow;
    bool track_overflow;
} spc_library_catalog_t;

typedef enum {
    SPC_CATALOG_ACCEPTED = 0,
    SPC_CATALOG_IGNORED,
    SPC_CATALOG_INVALID,
    SPC_CATALOG_FULL,
} spc_catalog_result_t;

void spc_library_init(spc_library_catalog_t *catalog);
spc_catalog_result_t spc_library_add_folder(spc_library_catalog_t *catalog, const char *name,
                                            bool hidden_or_system);
void spc_library_begin_folder(spc_library_catalog_t *catalog, uint16_t folder_index);
spc_catalog_result_t spc_library_add_track(spc_library_catalog_t *catalog, const char *filename,
                                           uint32_t file_size, bool hidden_or_system,
                                           bool is_directory);
void spc_library_sort_folders(spc_library_catalog_t *catalog);
void spc_library_sort_tracks(spc_library_catalog_t *catalog);
bool spc_library_track_ref_valid(const spc_library_catalog_t *catalog, spc_track_ref_t track);
bool visible_title_matches(const visible_title_t *title, uint32_t page_revision,
                           spc_track_ref_t track, uint8_t row);
bool storage_should_show_embedded(bool card_detected_at_boot);

#endif
