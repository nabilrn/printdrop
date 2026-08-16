#ifndef PRINTDROP_FILE_BEGIN_H
#define PRINTDROP_FILE_BEGIN_H

#include "printdrop/filename.h"
#include "printdrop/integrity.h"

#include <stddef.h>
#include <stdint.h>

#define PD_FILE_BEGIN_FIXED_SIZE 42U
#define PD_FILE_BEGIN_MAX_PAYLOAD (PD_FILE_BEGIN_FIXED_SIZE + PD_FILENAME_MAX_BYTES)

typedef enum pd_file_begin_result {
    PD_FILE_BEGIN_OK = 0,
    PD_FILE_BEGIN_INVALID_ARGUMENT,
    PD_FILE_BEGIN_INVALID_FILENAME,
    PD_FILE_BEGIN_BUFFER_TOO_SMALL,
    PD_FILE_BEGIN_MALFORMED
} pd_file_begin_result;

typedef struct pd_file_begin {
    uint64_t file_size;
    uint8_t sha256[PD_SHA256_BYTES];
    char filename[PD_FILENAME_CAPACITY];
} pd_file_begin;

pd_file_begin_result pd_file_begin_encode(const pd_file_begin *metadata,
                                          uint8_t *output,
                                          size_t output_capacity,
                                          size_t *bytes_written);
pd_file_begin_result pd_file_begin_decode(const uint8_t *payload,
                                          size_t payload_size,
                                          pd_file_begin *metadata);

#endif
