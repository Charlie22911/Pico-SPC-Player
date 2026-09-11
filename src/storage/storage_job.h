#ifndef STORAGE_STORAGE_JOB_H
#define STORAGE_STORAGE_JOB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ff.h"
#include "spc/spc_metadata.h"
#include "spc_catalog.h"

typedef enum {
    STORAGE_JOB_IDLE = 0,
    STORAGE_JOB_SCAN_ROOT_OPEN,
    STORAGE_JOB_SCAN_ROOT_ENTRY,
    STORAGE_JOB_SCAN_FOLDER_OPEN,
    STORAGE_JOB_SCAN_FOLDER_ENTRY,
    STORAGE_JOB_LOAD_OPEN,
    STORAGE_JOB_LOAD_CHUNK,
    STORAGE_JOB_LOAD_VALIDATE,
    STORAGE_JOB_LOAD_PUBLISH,
    STORAGE_JOB_WAIT_LOAD_ACK,
    STORAGE_JOB_VISIBLE_TITLE_OPEN,
    STORAGE_JOB_VISIBLE_TITLE_READ,
    STORAGE_JOB_VISIBLE_TITLE_PUBLISH,
} storage_job_state_t;

typedef struct {
    storage_job_state_t state;
    DIR directory;
    FIL file;
    char path[SPC_PATH_CAPACITY];
    size_t offset;
    size_t load_size;
    spc_track_ref_t target;
    spc_metadata_t metadata;
    uint8_t metadata_header[256];
    uint32_t page_revision;
    uint8_t row;
    visible_title_state_t title_state;
    FRESULT result;
    bool directory_open;
    bool file_open;
} storage_job_t;

void storage_job_init(storage_job_t *job);
void storage_job_close(storage_job_t *job);

#endif
