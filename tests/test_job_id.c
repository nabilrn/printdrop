#include "printdrop/job_id.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define PD_TEST_ASSERT(condition)                                                                    \
    do {                                                                                             \
        if (!(condition)) {                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                    \
            failures += 1;                                                                           \
        }                                                                                            \
    } while (0)

static bool fake_random(void *context, uint8_t *buffer, size_t buffer_size)
{
    size_t index;
    uint8_t seed = *(const uint8_t *)context;
    for (index = 0U; index < buffer_size; ++index) {
        buffer[index] = (uint8_t)(seed + (uint8_t)index);
    }
    return true;
}

static bool failing_random(void *context, uint8_t *buffer, size_t buffer_size)
{
    (void)context;
    (void)buffer;
    (void)buffer_size;
    return false;
}

int main(void)
{
    char job_id[PD_JOB_ID_CAPACITY];
    uint8_t seed = UINT8_C(16);

    PD_TEST_ASSERT(pd_job_id_create(job_id, fake_random, &seed) == PD_JOB_ID_OK);
    PD_TEST_ASSERT(strcmp(job_id, "101112131415161718191a1b1c1d1e1f") == 0);
    PD_TEST_ASSERT(pd_job_id_is_valid(job_id));
    PD_TEST_ASSERT(!pd_job_id_is_valid("101112131415161718191A1B1C1D1E1F"));
    PD_TEST_ASSERT(!pd_job_id_is_valid("../101112131415161718191a1b1c1d1e1f"));
    PD_TEST_ASSERT(pd_job_id_create(job_id, failing_random, NULL) == PD_JOB_ID_ENTROPY_FAILURE);
    PD_TEST_ASSERT(job_id[0] == '\0');

    if (failures != 0) {
        fprintf(stderr, "%d job id assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop job id tests passed.");
    return 0;
}
