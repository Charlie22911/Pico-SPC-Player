#include "psram_validation.h"

#define PSRAM_MANUFACTURER_ID 0x5du
#define MIB (1024u * 1024u)

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
        capacity_bytes < 3u * sizeof(uint32_t) ||
        capacity_bytes > BOARD_PSRAM_CAPACITY_LIMIT_BYTES ||
        (capacity_bytes % sizeof(uint32_t)) != 0u) {
        return false;
    }

    const size_t offsets[3] = {
        0u,
        (capacity_bytes / 2u) & ~(sizeof(uint32_t) - 1u),
        capacity_bytes - sizeof(uint32_t),
    };
    const uint32_t patterns[3] = {0x13579bdfu, 0x2468ace0u, 0xa55a3cc3u};
    uint32_t original[3];
    bool valid = true;
    size_t written = 0u;

    for (size_t i = 0u; i < 3u; ++i) {
        if (!access->read_word(access->context, offsets[i], &original[i])) {
            return false;
        }
    }

    for (; written < 3u; ++written) {
        if (!access->write_word(access->context, offsets[written], patterns[written])) {
            valid = false;
            break;
        }
    }

    if (valid) {
        for (size_t i = 0u; i < 3u; ++i) {
            uint32_t observed = 0u;
            if (!access->read_word(access->context, offsets[i], &observed) ||
                observed != patterns[i]) {
                valid = false;
                break;
            }
        }
    }

    for (size_t i = written; i > 0u; --i) {
        if (!access->write_word(access->context, offsets[i - 1u], original[i - 1u])) {
            valid = false;
        }
    }

    return valid;
}
