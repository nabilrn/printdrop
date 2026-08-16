#ifndef PRINTDROP_RELAY_MESSAGE_H
#define PRINTDROP_RELAY_MESSAGE_H

#include "printdrop/frame.h"

#include <stddef.h>
#include <stdint.h>

#define PD_RELAY_WS_MESSAGE_MAX_SIZE (60U * 1024U)
#define PD_RELAY_MAX_PAYLOAD (PD_RELAY_WS_MESSAGE_MAX_SIZE - PD_FRAME_HEADER_SIZE)

typedef enum pd_relay_message_result {
    PD_RELAY_MESSAGE_OK = 0,
    PD_RELAY_MESSAGE_INVALID_ARGUMENT,
    PD_RELAY_MESSAGE_PAYLOAD_TOO_LARGE,
    PD_RELAY_MESSAGE_BUFFER_TOO_SMALL,
    PD_RELAY_MESSAGE_MALFORMED
} pd_relay_message_result;

typedef struct pd_relay_message_view {
    pd_frame_header header;
    const uint8_t *payload;
    size_t payload_size;
} pd_relay_message_view;

pd_relay_message_result pd_relay_message_encode(const pd_frame_header *header,
                                                const uint8_t *payload,
                                                size_t payload_size,
                                                uint8_t *output,
                                                size_t output_capacity,
                                                size_t *bytes_written);
pd_relay_message_result pd_relay_message_decode(const uint8_t *message,
                                                size_t message_size,
                                                pd_relay_message_view *view);

#endif
