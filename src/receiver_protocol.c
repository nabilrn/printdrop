#include "printdrop/receiver_protocol.h"

#include "printdrop/filename.h"
#include "printdrop/protocol.h"

#include <string.h>

static bool pd_receiver_protocol_ops_valid(const pd_receiver_protocol_ops *ops)
{
    return ops != NULL && ops->begin_file != NULL && ops->write_chunk != NULL &&
           ops->finish_file != NULL && ops->abort_file != NULL;
}

static pd_receiver_protocol_result pd_receiver_protocol_fail(
    pd_receiver_protocol *protocol,
    pd_receiver_protocol_result result)
{
    if (protocol->file_active) {
        protocol->ops->abort_file(protocol->context);
        protocol->file_active = false;
    }
    protocol->state = PD_RECEIVER_PROTOCOL_FAILED;
    return result;
}

pd_receiver_protocol_result pd_receiver_protocol_init(pd_receiver_protocol *protocol,
                                                      const pd_receiver_protocol_ops *ops,
                                                      void *context)
{
    if (protocol == NULL || !pd_receiver_protocol_ops_valid(ops)) {
        return PD_RECEIVER_PROTOCOL_INVALID_ARGUMENT;
    }

    memset(protocol, 0, sizeof(*protocol));
    protocol->ops = ops;
    protocol->context = context;
    protocol->state = PD_RECEIVER_PROTOCOL_WAIT_FILE;
    return PD_RECEIVER_PROTOCOL_OK;
}

static pd_receiver_protocol_result pd_receiver_protocol_begin_file(
    pd_receiver_protocol *protocol,
    const uint8_t *payload,
    size_t payload_size)
{
    pd_file_begin metadata;
    char sanitized[PD_FILENAME_CAPACITY];

    if (pd_file_begin_decode(payload, payload_size, &metadata) != PD_FILE_BEGIN_OK) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_INVALID_METADATA);
    }

    if (pd_filename_sanitize(metadata.filename,
                             sanitized,
                             sizeof(sanitized),
                             NULL) != PD_FILENAME_OK) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_INVALID_METADATA);
    }

    if (!protocol->ops->begin_file(protocol->context, &metadata, sanitized)) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_HANDLER_ERROR);
    }

    protocol->current_file = metadata;
    memcpy(protocol->sanitized_filename, sanitized, strlen(sanitized) + 1U);
    protocol->file_active = true;
    protocol->state = PD_RECEIVER_PROTOCOL_RECEIVING;
    return PD_RECEIVER_PROTOCOL_OK;
}

static pd_receiver_protocol_result pd_receiver_protocol_write_chunk(
    pd_receiver_protocol *protocol,
    const uint8_t *payload,
    size_t payload_size)
{
    if (payload == NULL || payload_size == 0U || payload_size > (size_t)PD_FRAME_MAX_PAYLOAD) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_MALFORMED_FRAME);
    }

    if (!protocol->ops->write_chunk(protocol->context, payload, payload_size)) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_HANDLER_ERROR);
    }

    return PD_RECEIVER_PROTOCOL_OK;
}

static pd_receiver_protocol_result pd_receiver_protocol_finish_file(
    pd_receiver_protocol *protocol,
    size_t payload_size)
{
    if (payload_size != 0U) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_MALFORMED_FRAME);
    }

    if (!protocol->ops->finish_file(protocol->context)) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_HANDLER_ERROR);
    }

    protocol->file_active = false;
    protocol->state = PD_RECEIVER_PROTOCOL_COMPLETE;
    return PD_RECEIVER_PROTOCOL_OK;
}

pd_receiver_protocol_result pd_receiver_protocol_handle(pd_receiver_protocol *protocol,
                                                        const pd_frame_header *header,
                                                        const uint8_t *payload,
                                                        size_t payload_size)
{
    if (protocol == NULL || header == NULL ||
        (payload == NULL && payload_size != 0U) ||
        !pd_receiver_protocol_ops_valid(protocol->ops)) {
        return PD_RECEIVER_PROTOCOL_INVALID_ARGUMENT;
    }

    if (protocol->state == PD_RECEIVER_PROTOCOL_COMPLETE ||
        protocol->state == PD_RECEIVER_PROTOCOL_FAILED) {
        return PD_RECEIVER_PROTOCOL_INVALID_STATE;
    }

    if (!pd_message_type_is_valid((uint8_t)header->type) || header->flags != UINT16_C(0) ||
        header->payload_length > (uint32_t)PD_FRAME_MAX_PAYLOAD ||
        (size_t)header->payload_length != payload_size) {
        return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_MALFORMED_FRAME);
    }

    if (protocol->state == PD_RECEIVER_PROTOCOL_WAIT_FILE) {
        if (header->type != PD_MSG_FILE_BEGIN) {
            return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_UNEXPECTED_MESSAGE);
        }
        return pd_receiver_protocol_begin_file(protocol, payload, payload_size);
    }

    if (header->type == PD_MSG_CHUNK) {
        return pd_receiver_protocol_write_chunk(protocol, payload, payload_size);
    }
    if (header->type == PD_MSG_FILE_END) {
        return pd_receiver_protocol_finish_file(protocol, payload_size);
    }

    return pd_receiver_protocol_fail(protocol, PD_RECEIVER_PROTOCOL_UNEXPECTED_MESSAGE);
}

void pd_receiver_protocol_abort(pd_receiver_protocol *protocol)
{
    if (protocol == NULL || protocol->state == PD_RECEIVER_PROTOCOL_COMPLETE ||
        protocol->state == PD_RECEIVER_PROTOCOL_FAILED) {
        return;
    }

    if (protocol->file_active) {
        protocol->ops->abort_file(protocol->context);
        protocol->file_active = false;
    }
    protocol->state = PD_RECEIVER_PROTOCOL_FAILED;
}
