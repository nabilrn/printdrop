#ifndef PRINTDROP_FRAME_H
#define PRINTDROP_FRAME_H

#include "printdrop/protocol.h"

#include <stddef.h>
#include <stdint.h>

#define PD_FRAME_HEADER_SIZE 12U
#define PD_FRAME_MAX_PAYLOAD (1024U * 1024U)

typedef enum pd_frame_result {
    PD_FRAME_OK = 0,
    PD_FRAME_INVALID_ARGUMENT,
    PD_FRAME_BAD_MAGIC,
    PD_FRAME_UNSUPPORTED_VERSION,
    PD_FRAME_INVALID_TYPE,
    PD_FRAME_UNSUPPORTED_FLAGS,
    PD_FRAME_PAYLOAD_TOO_LARGE
} pd_frame_result;

typedef struct pd_frame_header {
    pd_message_type type;
    uint16_t flags;
    uint32_t payload_length;
} pd_frame_header;

pd_frame_result pd_frame_encode_header(const pd_frame_header *header,
                                       uint8_t output[PD_FRAME_HEADER_SIZE]);
pd_frame_result pd_frame_decode_header(const uint8_t input[PD_FRAME_HEADER_SIZE],
                                       pd_frame_header *header);

#endif
