#include "printdrop/ack.h"

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

static bool pd_ack_type_is_allowed(pd_message_type type)
{
    return type == PD_MSG_FILE_BEGIN || type == PD_MSG_CHUNK || type == PD_MSG_FILE_END;
}

static bool pd_ack_status_is_valid(uint8_t status)
{
    return status <= (uint8_t)PD_ACK_REJECTED;
}

pd_ack_result pd_ack_encode(const pd_ack *ack, uint8_t output[PD_ACK_PAYLOAD_SIZE])
{
    if (ack == NULL || output == NULL) {
        return PD_ACK_INVALID_ARGUMENT;
    }
    if (!pd_ack_type_is_allowed(ack->acknowledged_type)) {
        return PD_ACK_INVALID_TYPE;
    }
    if (!pd_ack_status_is_valid((uint8_t)ack->status)) {
        return PD_ACK_INVALID_STATUS;
    }

    output[0] = (uint8_t)ack->acknowledged_type;
    output[1] = (uint8_t)ack->status;
    output[2] = UINT8_C(0);
    output[3] = UINT8_C(0);
    pd_write_u64_be(&output[4], ack->received_bytes);
    return PD_ACK_RESULT_OK;
}

pd_ack_result pd_ack_decode(const uint8_t input[PD_ACK_PAYLOAD_SIZE],
                            size_t input_size,
                            pd_ack *ack)
{
    pd_message_type type;
    uint8_t status;

    if (input == NULL || ack == NULL) {
        return PD_ACK_INVALID_ARGUMENT;
    }
    if (input_size != (size_t)PD_ACK_PAYLOAD_SIZE || input[2] != UINT8_C(0) ||
        input[3] != UINT8_C(0)) {
        return PD_ACK_MALFORMED;
    }

    type = (pd_message_type)input[0];
    status = input[1];
    if (!pd_ack_type_is_allowed(type)) {
        return PD_ACK_INVALID_TYPE;
    }
    if (!pd_ack_status_is_valid(status)) {
        return PD_ACK_INVALID_STATUS;
    }

    ack->acknowledged_type = type;
    ack->status = (pd_ack_status)status;
    ack->received_bytes = pd_read_u64_be(&input[4]);
    return PD_ACK_RESULT_OK;
}
