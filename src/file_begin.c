#include "printdrop/file_begin.h"

#include <string.h>

static void pd_write_u16_be(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static uint16_t pd_read_u16_be(const uint8_t *input)
{
    return (uint16_t)(((uint16_t)input[0] << 8U) | (uint16_t)input[1]);
}

static void pd_write_u64_be(uint8_t *output, uint64_t value)
{
    output[0] = (uint8_t)(value >> 56U);
    output[1] = (uint8_t)(value >> 48U);
    output[2] = (uint8_t)(value >> 40U);
    output[3] = (uint8_t)(value >> 32U);
    output[4] = (uint8_t)(value >> 24U);
    output[5] = (uint8_t)(value >> 16U);
    output[6] = (uint8_t)(value >> 8U);
    output[7] = (uint8_t)value;
}

static uint64_t pd_read_u64_be(const uint8_t *input)
{
    return ((uint64_t)input[0] << 56U) | ((uint64_t)input[1] << 48U) |
           ((uint64_t)input[2] << 40U) | ((uint64_t)input[3] << 32U) |
           ((uint64_t)input[4] << 24U) | ((uint64_t)input[5] << 16U) |
           ((uint64_t)input[6] << 8U) | (uint64_t)input[7];
}

pd_file_begin_result pd_file_begin_encode(const pd_file_begin *metadata,
                                          uint8_t *output,
                                          size_t output_capacity,
                                          size_t *bytes_written)
{
    size_t filename_length;
    size_t required;

    if (bytes_written != NULL) {
        *bytes_written = 0U;
    }

    if (metadata == NULL || output == NULL) {
        return PD_FILE_BEGIN_INVALID_ARGUMENT;
    }

    filename_length = strlen(metadata->filename);
    if (filename_length == 0U || filename_length > (size_t)PD_FILENAME_MAX_BYTES) {
        return PD_FILE_BEGIN_INVALID_FILENAME;
    }

    required = (size_t)PD_FILE_BEGIN_FIXED_SIZE + filename_length;
    if (output_capacity < required) {
        return PD_FILE_BEGIN_BUFFER_TOO_SMALL;
    }

    pd_write_u64_be(output, metadata->file_size);
    memcpy(&output[8], metadata->sha256, (size_t)PD_SHA256_BYTES);
    pd_write_u16_be(&output[40], (uint16_t)filename_length);
    memcpy(&output[PD_FILE_BEGIN_FIXED_SIZE], metadata->filename, filename_length);

    if (bytes_written != NULL) {
        *bytes_written = required;
    }
    return PD_FILE_BEGIN_OK;
}

pd_file_begin_result pd_file_begin_decode(const uint8_t *payload,
                                          size_t payload_size,
                                          pd_file_begin *metadata)
{
    uint16_t filename_length_u16;
    size_t filename_length;

    if (payload == NULL || metadata == NULL) {
        return PD_FILE_BEGIN_INVALID_ARGUMENT;
    }

    if (payload_size < (size_t)PD_FILE_BEGIN_FIXED_SIZE) {
        return PD_FILE_BEGIN_MALFORMED;
    }

    filename_length_u16 = pd_read_u16_be(&payload[40]);
    filename_length = (size_t)filename_length_u16;
    if (filename_length == 0U || filename_length > (size_t)PD_FILENAME_MAX_BYTES ||
        payload_size != (size_t)PD_FILE_BEGIN_FIXED_SIZE + filename_length) {
        return PD_FILE_BEGIN_MALFORMED;
    }

    if (memchr(&payload[PD_FILE_BEGIN_FIXED_SIZE], '\0', filename_length) != NULL) {
        return PD_FILE_BEGIN_MALFORMED;
    }

    memset(metadata, 0, sizeof(*metadata));
    metadata->file_size = pd_read_u64_be(payload);
    memcpy(metadata->sha256, &payload[8], (size_t)PD_SHA256_BYTES);
    memcpy(metadata->filename, &payload[PD_FILE_BEGIN_FIXED_SIZE], filename_length);
    metadata->filename[filename_length] = '\0';
    return PD_FILE_BEGIN_OK;
}
