#include "printdrop/receiver_loop.h"

#include "printdrop/ack.h"

static uint64_t pd_loop_received_bytes(const pd_receiver_loop *loop)
{
    return loop->progress == NULL ? UINT64_C(0) : loop->progress(loop->progress_context);
}

static void pd_loop_emit(pd_receiver_loop *loop, pd_receiver_loop_event event)
{
    if (loop->event_callback != NULL) {
        loop->event_callback(loop->event_context, event, pd_loop_received_bytes(loop));
    }
}

static bool pd_loop_send_ack(pd_receiver_loop *loop,
                             pd_message_type acknowledged_type,
                             pd_ack_status status)
{
    uint8_t payload[PD_ACK_PAYLOAD_SIZE];
    pd_ack ack;
    pd_frame_header header;

    ack.acknowledged_type = acknowledged_type;
    ack.status = status;
    ack.received_bytes = pd_loop_received_bytes(loop);
    if (pd_ack_encode(&ack, payload) != PD_ACK_RESULT_OK) {
        return false;
    }
    header.type = PD_MSG_ACK;
    header.flags = UINT16_C(0);
    header.payload_length = (uint32_t)PD_ACK_PAYLOAD_SIZE;
    return loop->io_ops->send_frame(loop->io_context,
                                    &header,
                                    payload,
                                    (size_t)PD_ACK_PAYLOAD_SIZE);
}

static bool pd_loop_should_ack(pd_message_type type)
{
    return type == PD_MSG_FILE_BEGIN || type == PD_MSG_CHUNK || type == PD_MSG_FILE_END;
}

static pd_ack_status pd_loop_error_status(pd_receiver_protocol_result result)
{
    if (result == PD_RECEIVER_PROTOCOL_HANDLER_ERROR) {
        return PD_ACK_STORAGE_ERROR;
    }
    if (result == PD_RECEIVER_PROTOCOL_INVALID_METADATA) {
        return PD_ACK_REJECTED;
    }
    return PD_ACK_PROTOCOL_ERROR;
}

pd_receiver_loop_result pd_receiver_loop_run(pd_receiver_loop *loop)
{
    if (loop == NULL || loop->io_ops == NULL || loop->io_ops->receive_frame == NULL ||
        loop->io_ops->send_frame == NULL || loop->protocol == NULL) {
        return PD_RECEIVER_LOOP_INVALID_ARGUMENT;
    }

    pd_loop_emit(loop, PD_RECEIVER_LOOP_READY);
    for (;;) {
        pd_frame_header header;
        const uint8_t *payload = NULL;
        size_t payload_size = 0U;
        pd_receiver_protocol_result protocol_result;

        if (!loop->io_ops->receive_frame(loop->io_context,
                                         &header,
                                         &payload,
                                         &payload_size)) {
            pd_receiver_protocol_abort(loop->protocol);
            pd_loop_emit(loop, PD_RECEIVER_LOOP_FAILED);
            return PD_RECEIVER_LOOP_RECEIVE_ERROR;
        }
        if (header.type == PD_MSG_FILE_END) {
            pd_loop_emit(loop, PD_RECEIVER_LOOP_VERIFYING);
        }

        protocol_result = pd_receiver_protocol_handle(loop->protocol,
                                                      &header,
                                                      payload,
                                                      payload_size);
        if (protocol_result != PD_RECEIVER_PROTOCOL_OK) {
            if (pd_loop_should_ack(header.type)) {
                (void)pd_loop_send_ack(loop,
                                       header.type,
                                       pd_loop_error_status(protocol_result));
            }
            pd_receiver_protocol_abort(loop->protocol);
            pd_loop_emit(loop, PD_RECEIVER_LOOP_FAILED);
            return PD_RECEIVER_LOOP_PROTOCOL_ERROR;
        }
        if (pd_loop_should_ack(header.type) &&
            !pd_loop_send_ack(loop, header.type, PD_ACK_OK)) {
            pd_receiver_protocol_abort(loop->protocol);
            pd_loop_emit(loop, PD_RECEIVER_LOOP_FAILED);
            return PD_RECEIVER_LOOP_SEND_ERROR;
        }

        if (header.type == PD_MSG_FILE_BEGIN) {
            pd_loop_emit(loop, PD_RECEIVER_LOOP_FILE_STARTED);
        } else if (header.type == PD_MSG_CHUNK) {
            pd_loop_emit(loop, PD_RECEIVER_LOOP_PROGRESS);
        } else if (header.type == PD_MSG_FILE_END) {
            pd_loop_emit(loop, PD_RECEIVER_LOOP_COMPLETE);
            return PD_RECEIVER_LOOP_OK;
        }
    }
}
