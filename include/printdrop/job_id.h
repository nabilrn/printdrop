#ifndef PRINTDROP_JOB_ID_H
#define PRINTDROP_JOB_ID_H

#include "printdrop/receiver_session.h"

#include <stdbool.h>

#define PD_JOB_ID_BYTES 16U
#define PD_JOB_ID_HEX_CHARS (PD_JOB_ID_BYTES * 2U)
#define PD_JOB_ID_CAPACITY (PD_JOB_ID_HEX_CHARS + 1U)

typedef enum pd_job_id_result {
    PD_JOB_ID_OK = 0,
    PD_JOB_ID_INVALID_ARGUMENT,
    PD_JOB_ID_ENTROPY_FAILURE
} pd_job_id_result;

pd_job_id_result pd_job_id_create(char output[PD_JOB_ID_CAPACITY],
                                  pd_random_fill_fn random_fill,
                                  void *random_context);
bool pd_job_id_is_valid(const char *value);

#endif
