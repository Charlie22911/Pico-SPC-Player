#ifndef EMBEDDED_SPC_H
#define EMBEDDED_SPC_H

#include <stddef.h>
#include <stdint.h>

extern const uint8_t embedded_spc_start[];
extern const uint8_t embedded_spc_end[];

static inline size_t embedded_spc_size(void) {
    return (size_t)(embedded_spc_end - embedded_spc_start);
}

#endif
