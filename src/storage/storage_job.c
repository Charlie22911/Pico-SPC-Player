#include "storage_job.h"

#include <string.h>

void storage_job_init(storage_job_t *job) {
    if (job == NULL)
        return;
    memset(job, 0, sizeof(*job));
    job->state = STORAGE_JOB_IDLE;
    job->result = FR_OK;
}

void storage_job_close(storage_job_t *job) {
    if (job == NULL)
        return;
    if (job->directory_open) {
        (void)f_closedir(&job->directory);
        job->directory_open = false;
    }
    if (job->file_open) {
        (void)f_close(&job->file);
        job->file_open = false;
    }
}
