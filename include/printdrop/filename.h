#ifndef PRINTDROP_FILENAME_H
#define PRINTDROP_FILENAME_H

#include <stddef.h>

#define PD_FILENAME_MAX_BYTES 120U
#define PD_FILENAME_CAPACITY (PD_FILENAME_MAX_BYTES + 1U)

typedef enum pd_filename_result {
    PD_FILENAME_OK = 0,
    PD_FILENAME_INVALID_ARGUMENT,
    PD_FILENAME_TOO_LONG,
    PD_FILENAME_BUFFER_TOO_SMALL
} pd_filename_result;

pd_filename_result pd_filename_sanitize(const char *input,
                                        char *output,
                                        size_t output_capacity,
                                        size_t *required_capacity);

#endif
