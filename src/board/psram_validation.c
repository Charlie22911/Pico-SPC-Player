#include "psram_validation.h"

#define PSRAM_MANUFACTURER_ID 0x5du
#define MIB (1024u * 1024u)
#define PROBE_WORDS 16u
#define PROBE_BYTES (PROBE_WORDS * sizeof(uint32_t))
#define PROBE_REGIONS 3u

size_t board_psram_capacity_from_id(uint8_t manufacturer_id, uint8_t extended_id) {
    if (manufacturer_id != PSRAM_MANUFACTURER_ID) {
        return 0u;
    }

    size_t detected_bytes;
    const uint8_t size_id = (uint8_t)(extended_id >> 5u);
    if (extended_id == 0x26u || size_id == 2u) {
        detected_bytes = 8u * MIB;
    } else if (size_id == 1u) {
        detected_bytes = 4u * MIB;
    } else if (size_id == 0u) {
        detected_bytes = 1u * MIB;
    } else {
        return 0u;
    }

    return detected_bytes > BOARD_PSRAM_CAPACITY_LIMIT_BYTES ? BOARD_PSRAM_CAPACITY_LIMIT_BYTES
                                                             : detected_bytes;
}

bool board_psram_validate_memory(const board_psram_access_t *access, size_t capacity_bytes) {
    if (access == NULL || access->read_word == NULL || access->write_word == NULL ||
        capacity_bytes < PROBE_REGIONS * PROBE_BYTES ||
        capacity_bytes > BOARD_PSRAM_CAPACITY_LIMIT_BYTES ||
        (capacity_bytes % sizeof(uint32_t)) != 0u) {
        return false;
    }

    /* Exercise adjacent words and both sides of a 32-byte burst boundary. */
    const size_t offsets[PROBE_REGIONS] = {
        0u,
        (capacity_bytes / 2u) & ~(PROBE_BYTES - 1u),
        capacity_bytes - PROBE_BYTES,
    };
    const uint32_t patterns[3] = {0x13579bdfu, 0x2468ace0u, 0xa55a3cc3u};
    uint32_t original[PROBE_REGIONS * PROBE_WORDS];
    bool valid = true;
    size_t written = 0u;

    for (size_t i = 0u; i < PROBE_REGIONS * PROBE_WORDS; ++i) {
        const size_t offset = offsets[i / PROBE_WORDS] + (i % PROBE_WORDS) * sizeof(uint32_t);
        if (!access->read_word(access->context, offset, &original[i])) {
            return false;
        }
    }

    for (; written < PROBE_REGIONS * PROBE_WORDS; ++written) {
        const size_t word = written % PROBE_WORDS;
        const size_t region = written / PROBE_WORDS;
        const uint32_t pattern = patterns[region] ^ ((uint32_t)word * 0x01020408u);
        if (!access->write_word(access->context, offsets[region] + word * sizeof(uint32_t), pattern)) {
            valid = false;
            break;
        }
    }

    if (valid) {
        for (size_t i = 0u; i < PROBE_REGIONS * PROBE_WORDS; ++i) {
            const size_t word = i % PROBE_WORDS;
            const size_t region = i / PROBE_WORDS;
            const uint32_t pattern = patterns[region] ^ ((uint32_t)word * 0x01020408u);
            uint32_t observed = 0u;
            if (!access->read_word(access->context, offsets[region] + word * sizeof(uint32_t), &observed) ||
                observed != pattern) {
                valid = false;
                break;
            }
        }
    }

    for (size_t i = written; i > 0u; --i) {
        const size_t word = (i - 1u) % PROBE_WORDS;
        const size_t region = (i - 1u) / PROBE_WORDS;
        if (!access->write_word(access->context, offsets[region] + word * sizeof(uint32_t), original[i - 1u])) {
            valid = false;
        }
    }

    return valid;
}
