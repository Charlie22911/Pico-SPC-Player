#include "spc/spc_metadata.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { TEST_HEADER_SIZE = 256 };

static void make_header(uint8_t *data) {
    memset(data, 0, TEST_HEADER_SIZE);
    memcpy(data, "SNES-SPC700 Sound File Data v0.30", 33u);
    data[0x21] = 0x1au;
    data[0x22] = 0x1au;
    data[0x23] = 0x1au;
    data[0x24] = 30u;
}

static void test_text_id666(void) {
    uint8_t data[TEST_HEADER_SIZE];
    make_header(data);
    memcpy(data + 0x2e, "Test Song", 9u);
    memcpy(data + 0x4e, "Test Game", 9u);
    memcpy(data + 0x6e, "Test Dumper", 11u);
    memcpy(data + 0x7e, "A comment", 9u);
    memcpy(data + 0x9e, "09/09/2026", 10u);
    memcpy(data + 0xa9, "123", 3u);
    memcpy(data + 0xac, "04500", 5u);
    memcpy(data + 0xb1, "Test Artist", 11u);

    spc_metadata_t metadata;
    assert(spc_metadata_parse(data, sizeof(data), &metadata));
    assert(metadata.has_id666);
    assert(!metadata.binary_id666);
    assert(strcmp(metadata.title, "Test Song") == 0);
    assert(strcmp(metadata.game, "Test Game") == 0);
    assert(strcmp(metadata.artist, "Test Artist") == 0);
    assert(strcmp(metadata.dumper, "Test Dumper") == 0);
    assert(strcmp(metadata.comments, "A comment") == 0);
    assert(metadata.timing_valid);
    assert(metadata.song_seconds == 123u);
    assert(metadata.fade_milliseconds == 4500u);
}

static void test_binary_id666(void) {
    uint8_t data[TEST_HEADER_SIZE];
    make_header(data);
    memcpy(data + 0x2e, "Binary Song", 11u);
    data[0x9e] = 0xeau;
    data[0x9f] = 0x07u;
    data[0xa0] = 9u;
    data[0xa1] = 9u;
    data[0xa9] = 120u;
    data[0xaa] = 0u;
    data[0xab] = 0u;
    data[0xac] = 0x88u;
    data[0xad] = 0x13u;
    data[0xae] = 0u;
    data[0xaf] = 0u;
    memcpy(data + 0xb0, "Binary Artist", 13u);

    spc_metadata_t metadata;
    assert(spc_metadata_parse(data, sizeof(data), &metadata));
    assert(metadata.has_id666);
    assert(metadata.binary_id666);
    assert(strcmp(metadata.title, "Binary Song") == 0);
    assert(strcmp(metadata.artist, "Binary Artist") == 0);
    assert(metadata.timing_valid);
    assert(metadata.song_seconds == 120u);
    assert(metadata.fade_milliseconds == 5000u);
}

static void test_truncated_and_absent_tags(void) {
    uint8_t data[TEST_HEADER_SIZE];
    make_header(data);
    spc_metadata_t metadata;
    assert(!spc_metadata_parse(data, 0xd1u, &metadata));

    data[0x23] = 0x1bu;
    assert(spc_metadata_parse(data, sizeof(data), &metadata));
    assert(!metadata.has_id666);
    assert(metadata.title[0] == '\0');
}

static void test_malformed_text_timing_stays_text(void) {
    uint8_t data[TEST_HEADER_SIZE];
    make_header(data);
    memcpy(data + 0x9e, "09/09/2026", 10u);
    memcpy(data + 0xa9, "1X3", 3u);
    memcpy(data + 0xac, "04500", 5u);
    memcpy(data + 0xb1, "Text Artist", 11u);
    spc_metadata_t metadata;
    assert(spc_metadata_parse(data, sizeof(data), &metadata));
    assert(!metadata.binary_id666);
    assert(!metadata.timing_valid);
    assert(strcmp(metadata.artist, "Text Artist") == 0);
}

int main(void) {
    test_text_id666();
    test_binary_id666();
    test_truncated_and_absent_tags();
    test_malformed_text_timing_stays_text();
    puts("SPC metadata contract OK");
    return 0;
}
