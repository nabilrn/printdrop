#include "printdrop/relay_message.h"

#include <string.h>

pd_relay_message_result pd_relay_message_encode(const pd_frame_header *header,
                                                const uint8_t *payload,
                                                size_t payload_size,
                                                uint8_t *output,
                                                size_t output_capacity,
                                                size_t *bytes_written)
{
    size_t required;

    if (bytes_written != NULL) {
        *bytes_written = 0U;
    }

    if (header == NULL || output == NULL || (payload == NULL && payload_size != 0U)) {
        return PD_RELAY_MESSAGE_INVALID_ARGUMENT;
    }
    if (payload_size > (size_t)PD_RELAY_MAX_PAYLOAD ||
        (size_t)header->payload_length != payload_size) {
        return PD_RELAY_MESSAGE_PAYLOAD_TOO_LARGE;
    }

    required = (size_t)PD_FRAME_HEADER_SIZE + payload_size;
    if (output_capacity < required) {
        return PD_RELAY_MESSAGE_BUFFER_TOO_SMALL;
    }
    if (pd_frame_encode_header(header, output) != PD_FRAME_OK) {
        return PD_RELAY_MESSAGE_MALFORMED;
    }
    if (payload_size != 0U) {
        memcpy(&output[PD_FRAME_HEADER_SIZE], payload, payload_size);
    }

    if (bytes_written != NULL) {
        *bytes_written = required;
    }
    return PD_RELAY_MESSAGE_OK;
}

pd_relay_message_result pd_relay_message_decode(const uint8_t *message,
                                                size_t message_size,
                                                pd_relay_message_view *view)
{
    pd_frame_header header;
    size_t payload_size;

    if (message == NULL || view == NULL) {
        return PD_RELAY_MESSAGE_INVALID_ARGUMENT;
    }
    if (message_size < (size_t)PD_FRAME_HEADER_SIZE ||
        message_size > (size_t)PD_RELAY_WS_MESSAGE_MAX_SIZE) {
        return PD_RELAY_MESSAGE_MALFORMED;
    }
    if (pd_frame_decode_header(message, &header) != PD_FRAME_OK) {
        return PD_RELAY_MESSAGE_MALFORMED;
    }

    payload_size = message_size - (size_t)PD_FRAME_HEADER_SIZE;
    if (payload_size > (size_t)PD_RELAY_MAX_PAYLOAD ||
        (size_t)header.payload_length != payload_size) {
        return PD_RELAY_MESSAGE_MALFORMED;
    }

    view->header = header;
    view->payload = payload_size == 0U ? NULL : &message[PD_FRAME_HEADER_SIZE];
    view->payload_size = payload_size;
    return PD_RELAY_MESSAGE_OK;
}
