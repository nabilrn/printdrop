#include "printdrop/job_id.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static char pd_job_hex_digit(uint8_t value)
{
    static const char digits[] = "0123456789abcdef";
    return digits[value & UINT8_C(0x0f)];
}

pd_job_id_result pd_job_id_create(char output[PD_JOB_ID_CAPACITY],
                                  pd_random_fill_fn random_fill,
                                  void *random_context)
{
    uint8_t random_bytes[PD_JOB_ID_BYTES];
    size_t index;

    if (output == NULL || random_fill == NULL) {
        return PD_JOB_ID_INVALID_ARGUMENT;
    }

    output[0] = '\0';
    if (!random_fill(random_context, random_bytes, sizeof(random_bytes))) {
        return PD_JOB_ID_ENTROPY_FAILURE;
    }

    for (index = 0U; index < sizeof(random_bytes); ++index) {
        output[index * 2U] = pd_job_hex_digit((uint8_t)(random_bytes[index] >> 4U));
        output[(index * 2U) + 1U] = pd_job_hex_digit(random_bytes[index]);
    }
    output[PD_JOB_ID_HEX_CHARS] = '\0';
    return PD_JOB_ID_OK;
}

bool pd_job_id_is_valid(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != (size_t)PD_JOB_ID_HEX_CHARS) {
        return false;
    }

    for (index = 0U; index < (size_t)PD_JOB_ID_HEX_CHARS; ++index) {
        char character = value[index];
        bool digit = character >= '0' && character <= '9';
        bool lower_hex = character >= 'a' && character <= 'f';
        if (!digit && !lower_hex) {
            return false;
        }
    }

    return true;
}
