#ifndef PRINTDROP_ACK_H
#define PRINTDROP_ACK_H

#include "printdrop/protocol.h"

#include <stddef.h>
#include <stdint.h>

#define PD_ACK_PAYLOAD_SIZE 12U

typedef enum pd_ack_status {
    PD_ACK_OK = 0,
    PD_ACK_PROTOCOL_ERROR = 1,
    PD_ACK_STORAGE_ERROR = 2,
    PD_ACK_INTEGRITY_ERROR = 3,
    PD_ACK_REJECTED = 4
} pd_ack_status;

typedef enum pd_ack_result {
    PD_ACK_RESULT_OK = 0,
    PD_ACK_INVALID_ARGUMENT,
    PD_ACK_INVALID_TYPE,
    PD_ACK_INVALID_STATUS,
    PD_ACK_MALFORMED
} pd_ack_result;

typedef struct pd_ack {
    pd_message_type acknowledged_type;
    pd_ack_status status;
    uint64_t received_bytes;
} pd_ack;

pd_ack_result pd_ack_encode(const pd_ack *ack, uint8_t output[PD_ACK_PAYLOAD_SIZE]);
pd_ack_result pd_ack_decode(const uint8_t input[PD_ACK_PAYLOAD_SIZE],
                            size_t input_size,
                            pd_ack *ack);

#endif
