#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "storage/spc_catalog.h"
#include "storage/storage_mailbox.h"

static void test_catalog(void) {
    spc_library_catalog_t catalog;
    spc_library_init(&catalog);
    assert(spc_library_add_folder(&catalog, "z game", false) == SPC_CATALOG_ACCEPTED);
    assert(spc_library_add_folder(&catalog, "A Game", false) == SPC_CATALOG_ACCEPTED);
    assert(spc_library_add_folder(&catalog, ".hidden", false) == SPC_CATALOG_IGNORED);
    spc_library_sort_folders(&catalog);
    assert(strcmp(catalog.folders[0].name, "A Game") == 0);
    assert(strcmp(catalog.folders[1].name, "z game") == 0);

    spc_library_begin_folder(&catalog, 0u);
    assert(spc_library_add_track(&catalog, "notes.txt", 66092u, false, false) ==
           SPC_CATALOG_IGNORED);
    assert(spc_library_add_track(&catalog, "short.spc", 123u, false, false) == SPC_CATALOG_INVALID);
    assert(spc_library_add_track(&catalog, "B.SPC", 1024u * 1024u, false, false) ==
           SPC_CATALOG_ACCEPTED);
    assert(spc_library_add_track(&catalog, "A.SpC", 66092u, false, false) == SPC_CATALOG_ACCEPTED);
    spc_library_sort_tracks(&catalog);
    assert(strcmp(catalog.tracks[0].filename, "A.SpC") == 0);

    for (uint32_t i = catalog.track_count; i < SPC_TRACK_CAPACITY; ++i) {
        char name[24];
        (void)snprintf(name, sizeof(name), "track%03lu.spc", (unsigned long)i);
        assert(spc_library_add_track(&catalog, name, SPC_CORE_LOAD_BYTES, false, false) ==
               SPC_CATALOG_ACCEPTED);
    }
    assert(spc_library_add_track(&catalog, "overflow.spc", SPC_CORE_LOAD_BYTES, false, false) ==
           SPC_CATALOG_FULL);
    assert(catalog.track_overflow);
    assert(catalog.folder_count == 2u);
    assert(spc_library_add_folder(&catalog, "Later Game", false) == SPC_CATALOG_ACCEPTED);
    assert(catalog.folder_count == 3u);
    const spc_track_ref_t valid = {catalog.folder_revision, 1u};
    assert(spc_library_track_ref_valid(&catalog, valid));
    spc_library_begin_folder(&catalog, 1u);
    assert(!spc_library_track_ref_valid(&catalog, valid));
    const visible_title_t old_title = {
        .track = valid,
        .page_revision = 8u,
        .row = 1u,
        .state = VISIBLE_TITLE_ID666,
        .title = "OLD PAGE",
    };
    assert(visible_title_matches(&old_title, 8u, valid, 1u));
    assert(!visible_title_matches(&old_title, 9u, valid, 1u));
    assert(
        !visible_title_matches(&old_title, 8u, (spc_track_ref_t){valid.folder_revision, 0u}, 1u));
    assert(!storage_should_show_embedded(true));
    assert(storage_should_show_embedded(false));
}

static void test_mailbox(void) {
    storage_mailbox_t mailbox;
    storage_mailbox_init(&mailbox);
    uint8_t *write = storage_mailbox_begin_write(&mailbox);
    assert(write != NULL);
    assert(storage_mailbox_begin_write(&mailbox) == NULL);
    write[0] = 0x42u;
    storage_mailbox_publish(&mailbox, SPC_CORE_LOAD_BYTES, (spc_track_ref_t){9u, 7u});
    const uint8_t *read = NULL;
    size_t size = 0u;
    spc_track_ref_t track = {0};
    assert(storage_mailbox_begin_read(&mailbox, &read, &size, &track));
    assert(read[0] == 0x42u && size == SPC_CORE_LOAD_BYTES);
    assert(track.folder_revision == 9u && track.track_index == 7u);
    assert(!storage_mailbox_begin_read(&mailbox, &read, &size, &track));
    storage_mailbox_complete_read(&mailbox, true, 4u);
    bool accepted = false;
    uint32_t generation = 0u;
    assert(storage_mailbox_reclaim(&mailbox, &accepted, &generation));
    assert(accepted && generation == 4u);
    assert(storage_mailbox_begin_write(&mailbox) != NULL);
}

int main(void) {
    test_catalog();
    test_mailbox();
    puts("storage tests passed");
    return 0;
}
