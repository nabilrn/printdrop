#include "printdrop/frame.h"

#define PD_MAGIC_0 UINT8_C(0x50)
#define PD_MAGIC_1 UINT8_C(0x44)
#define PD_MAGIC_2 UINT8_C(0x52)
#define PD_MAGIC_3 UINT8_C(0x50)

static void pd_write_u16_be(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static void pd_write_u32_be(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

static uint16_t pd_read_u16_be(const uint8_t *input)
{
    return (uint16_t)(((uint16_t)input[0] << 8U) | (uint16_t)input[1]);
}

static uint32_t pd_read_u32_be(const uint8_t *input)
{
    return ((uint32_t)input[0] << 24U) | ((uint32_t)input[1] << 16U) |
           ((uint32_t)input[2] << 8U) | (uint32_t)input[3];
}

pd_frame_result pd_frame_encode_header(const pd_frame_header *header,
                                       uint8_t output[PD_FRAME_HEADER_SIZE])
{
    if (header == NULL || output == NULL) {
        return PD_FRAME_INVALID_ARGUMENT;
    }

    if (!pd_message_type_is_valid((uint8_t)header->type)) {
        return PD_FRAME_INVALID_TYPE;
    }

    if (header->flags != UINT16_C(0)) {
        return PD_FRAME_UNSUPPORTED_FLAGS;
    }

    if (header->payload_length > (uint32_t)PD_FRAME_MAX_PAYLOAD) {
        return PD_FRAME_PAYLOAD_TOO_LARGE;
    }

    output[0] = PD_MAGIC_0;
    output[1] = PD_MAGIC_1;
    output[2] = PD_MAGIC_2;
    output[3] = PD_MAGIC_3;
    output[4] = PD_PROTOCOL_VERSION;
    output[5] = (uint8_t)header->type;
    pd_write_u16_be(&output[6], header->flags);
    pd_write_u32_be(&output[8], header->payload_length);
    return PD_FRAME_OK;
}

pd_frame_result pd_frame_decode_header(const uint8_t input[PD_FRAME_HEADER_SIZE],
                                       pd_frame_header *header)
{
    uint8_t type;
    uint16_t flags;
    uint32_t payload_length;

    if (input == NULL || header == NULL) {
        return PD_FRAME_INVALID_ARGUMENT;
    }

    if (input[0] != PD_MAGIC_0 || input[1] != PD_MAGIC_1 || input[2] != PD_MAGIC_2 ||
        input[3] != PD_MAGIC_3) {
        return PD_FRAME_BAD_MAGIC;
    }

    if (input[4] != PD_PROTOCOL_VERSION) {
        return PD_FRAME_UNSUPPORTED_VERSION;
    }

    type = input[5];
    if (!pd_message_type_is_valid(type)) {
        return PD_FRAME_INVALID_TYPE;
    }

    flags = pd_read_u16_be(&input[6]);
    if (flags != UINT16_C(0)) {
        return PD_FRAME_UNSUPPORTED_FLAGS;
    }

    payload_length = pd_read_u32_be(&input[8]);
    if (payload_length > (uint32_t)PD_FRAME_MAX_PAYLOAD) {
        return PD_FRAME_PAYLOAD_TOO_LARGE;
    }

    header->type = (pd_message_type)type;
    header->flags = flags;
    header->payload_length = payload_length;
    return PD_FRAME_OK;
}
