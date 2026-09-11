#include "spc_catalog.h"

#include <stddef.h>
#include <string.h>

static char ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

static int ascii_case_compare(const char *left, const char *right) {
    for (;;) {
        const unsigned char a = (unsigned char)ascii_lower(*left++);
        const unsigned char b = (unsigned char)ascii_lower(*right++);
        if (a != b)
            return a < b ? -1 : 1;
        if (a == 0u)
            return 0;
    }
}

static bool valid_component(const char *name) {
    const size_t length = name == NULL ? 0u : strlen(name);
    return length != 0u && length < SPC_LFN_CAPACITY && strchr(name, '/') == NULL &&
           strchr(name, '\\') == NULL;
}

static bool has_spc_extension(const char *name) {
    const size_t length = name == NULL ? 0u : strlen(name);
    return length >= 4u && name[length - 4u] == '.' && ascii_lower(name[length - 3u]) == 's' &&
           ascii_lower(name[length - 2u]) == 'p' && ascii_lower(name[length - 1u]) == 'c';
}

void spc_library_init(spc_library_catalog_t *catalog) {
    if (catalog == NULL)
        return;
    *catalog = (spc_library_catalog_t){0};
    catalog->open_folder = UINT16_MAX;
}

spc_catalog_result_t spc_library_add_folder(spc_library_catalog_t *catalog, const char *name,
                                            bool hidden_or_system) {
    if (catalog == NULL || !valid_component(name))
        return SPC_CATALOG_INVALID;
    if (hidden_or_system || name[0] == '.')
        return SPC_CATALOG_IGNORED;
    for (uint16_t i = 0u; i < catalog->folder_count; ++i) {
        if (ascii_case_compare(catalog->folders[i].name, name) == 0)
            return SPC_CATALOG_IGNORED;
    }
    if (catalog->folder_count >= SPC_FOLDER_CAPACITY) {
        catalog->folder_overflow = true;
        return SPC_CATALOG_FULL;
    }
    (void)strcpy(catalog->folders[catalog->folder_count++].name, name);
    return SPC_CATALOG_ACCEPTED;
}

void spc_library_begin_folder(spc_library_catalog_t *catalog, uint16_t folder_index) {
    if (catalog == NULL)
        return;
    catalog->track_count = 0u;
    catalog->track_overflow = false;
    catalog->open_folder = folder_index < catalog->folder_count ? folder_index : UINT16_MAX;
    ++catalog->folder_revision;
    if (catalog->folder_revision == 0u)
        ++catalog->folder_revision;
}

spc_catalog_result_t spc_library_add_track(spc_library_catalog_t *catalog, const char *filename,
                                           uint32_t file_size, bool hidden_or_system,
                                           bool is_directory) {
    if (catalog == NULL || !valid_component(filename))
        return SPC_CATALOG_INVALID;
    if (hidden_or_system || is_directory || filename[0] == '.' || !has_spc_extension(filename))
        return SPC_CATALOG_IGNORED;
    if (file_size < SPC_CORE_MIN_BYTES)
        return SPC_CATALOG_INVALID;
    if (catalog->track_count >= SPC_TRACK_CAPACITY) {
        catalog->track_overflow = true;
        return SPC_CATALOG_FULL;
    }
    spc_track_entry_t *entry = &catalog->tracks[catalog->track_count++];
    (void)strcpy(entry->filename, filename);
    entry->file_size = file_size;
    return SPC_CATALOG_ACCEPTED;
}

void spc_library_sort_folders(spc_library_catalog_t *catalog) {
    if (catalog == NULL)
        return;
    for (uint16_t i = 1u; i < catalog->folder_count; ++i) {
        const spc_folder_entry_t value = catalog->folders[i];
        uint16_t position = i;
        while (position != 0u &&
               ascii_case_compare(catalog->folders[position - 1u].name, value.name) > 0) {
            catalog->folders[position] = catalog->folders[position - 1u];
            --position;
        }
        catalog->folders[position] = value;
    }
}

void spc_library_sort_tracks(spc_library_catalog_t *catalog) {
    if (catalog == NULL)
        return;
    for (uint16_t i = 1u; i < catalog->track_count; ++i) {
        const spc_track_entry_t value = catalog->tracks[i];
        uint16_t position = i;
        while (position != 0u &&
               ascii_case_compare(catalog->tracks[position - 1u].filename, value.filename) > 0) {
            catalog->tracks[position] = catalog->tracks[position - 1u];
            --position;
        }
        catalog->tracks[position] = value;
    }
}

bool spc_library_track_ref_valid(const spc_library_catalog_t *catalog, spc_track_ref_t track) {
    return catalog != NULL && catalog->open_folder < catalog->folder_count &&
           track.folder_revision == catalog->folder_revision &&
           track.track_index < catalog->track_count;
}

bool visible_title_matches(const visible_title_t *title, uint32_t page_revision,
                           spc_track_ref_t track, uint8_t row) {
    return title != NULL && title->page_revision == page_revision && title->row == row &&
           title->track.folder_revision == track.folder_revision &&
           title->track.track_index == track.track_index;
}

bool storage_should_show_embedded(bool card_detected_at_boot) {
    return !card_detected_at_boot;
}
