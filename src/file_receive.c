#include "printdrop/file_receive.h"

#include <string.h>

static bool pd_file_sink_is_configured(const pd_file_sink_ops *ops)
{
    return ops != NULL && ops->begin != NULL && ops->write != NULL && ops->commit != NULL &&
           ops->abort != NULL;
}

static bool pd_integrity_is_configured(const pd_integrity_ops *ops)
{
    return ops != NULL && ops->begin != NULL && ops->update != NULL && ops->finish != NULL &&
           ops->abort != NULL;
}

static void pd_file_receiver_abort_resources(pd_file_receiver *receiver)
{
    if (receiver->integrity_started) {
        receiver->integrity_ops->abort(receiver->integrity_context);
        receiver->integrity_started = false;
    }
    if (receiver->sink_started) {
        receiver->sink_ops->abort(receiver->sink_context);
        receiver->sink_started = false;
    }
}

static pd_file_receive_result pd_file_receiver_fail(pd_file_receiver *receiver,
                                                    pd_file_receive_result result)
{
    pd_file_receiver_abort_resources(receiver);
    receiver->state = PD_FILE_RECEIVE_FAILED;
    return result;
}

pd_file_receive_result pd_file_receiver_init(pd_file_receiver *receiver,
                                             const pd_file_sink_ops *sink_ops,
                                             void *sink_context,
                                             uint64_t expected_bytes)
{
    if (receiver == NULL || !pd_file_sink_is_configured(sink_ops)) {
        return PD_FILE_RECEIVE_INVALID_ARGUMENT;
    }

    memset(receiver, 0, sizeof(*receiver));
    receiver->sink_ops = sink_ops;
    receiver->sink_context = sink_context;
    receiver->expected_bytes = expected_bytes;
    receiver->state = PD_FILE_RECEIVE_IDLE;
    return PD_FILE_RECEIVE_OK;
}

pd_file_receive_result pd_file_receiver_configure_integrity(
    pd_file_receiver *receiver,
    const pd_integrity_ops *integrity_ops,
    void *integrity_context,
    const uint8_t expected_sha256[PD_SHA256_BYTES])
{
    if (receiver == NULL || !pd_integrity_is_configured(integrity_ops) || expected_sha256 == NULL) {
        return PD_FILE_RECEIVE_INVALID_ARGUMENT;
    }
    if (receiver->state != PD_FILE_RECEIVE_IDLE || receiver->sink_started) {
        return PD_FILE_RECEIVE_INVALID_STATE;
    }

    receiver->integrity_ops = integrity_ops;
    receiver->integrity_context = integrity_context;
    memcpy(receiver->expected_sha256, expected_sha256, sizeof(receiver->expected_sha256));
    receiver->integrity_configured = true;
    return PD_FILE_RECEIVE_OK;
}

pd_file_receive_result pd_file_receiver_begin(pd_file_receiver *receiver)
{
    if (receiver == NULL || !pd_file_sink_is_configured(receiver->sink_ops)) {
        return PD_FILE_RECEIVE_INVALID_ARGUMENT;
    }

    if (receiver->state != PD_FILE_RECEIVE_IDLE) {
        return PD_FILE_RECEIVE_INVALID_STATE;
    }

    if (receiver->sink_ops->begin(receiver->sink_context, receiver->expected_bytes) !=
        PD_FILE_SINK_OK) {
        receiver->state = PD_FILE_RECEIVE_FAILED;
        return PD_FILE_RECEIVE_SINK_ERROR;
    }
    receiver->sink_started = true;

    if (receiver->integrity_configured) {
        if (receiver->integrity_ops->begin(receiver->integrity_context) != PD_INTEGRITY_OK) {
            return pd_file_receiver_fail(receiver, PD_FILE_RECEIVE_INTEGRITY_ERROR);
        }
        receiver->integrity_started = true;
    }

    receiver->state = PD_FILE_RECEIVE_RECEIVING;
    return PD_FILE_RECEIVE_OK;
}

pd_file_receive_result pd_file_receiver_write(pd_file_receiver *receiver,
                                              const uint8_t *data,
                                              size_t data_size)
{
    size_t bytes_written = 0U;
    uint64_t data_size_u64;

    if (receiver == NULL || (data == NULL && data_size != 0U)) {
        return PD_FILE_RECEIVE_INVALID_ARGUMENT;
    }

    if (receiver->state != PD_FILE_RECEIVE_RECEIVING) {
        return PD_FILE_RECEIVE_INVALID_STATE;
    }

    if (data_size == 0U) {
        return PD_FILE_RECEIVE_INVALID_ARGUMENT;
    }

    if (data_size > (size_t)PD_FRAME_MAX_PAYLOAD) {
        return PD_FILE_RECEIVE_CHUNK_TOO_LARGE;
    }

    data_size_u64 = (uint64_t)data_size;
    if (receiver->received_bytes > receiver->expected_bytes ||
        data_size_u64 > receiver->expected_bytes - receiver->received_bytes) {
        return PD_FILE_RECEIVE_EXCEEDS_EXPECTED_SIZE;
    }

    if (receiver->sink_ops->write(receiver->sink_context, data, data_size, &bytes_written) !=
            PD_FILE_SINK_OK ||
        bytes_written != data_size) {
        return pd_file_receiver_fail(receiver, PD_FILE_RECEIVE_SINK_ERROR);
    }

    if (receiver->integrity_configured &&
        receiver->integrity_ops->update(receiver->integrity_context, data, data_size) !=
            PD_INTEGRITY_OK) {
        return pd_file_receiver_fail(receiver, PD_FILE_RECEIVE_INTEGRITY_ERROR);
    }

    receiver->received_bytes += data_size_u64;
    return PD_FILE_RECEIVE_OK;
}

pd_file_receive_result pd_file_receiver_finish(pd_file_receiver *receiver)
{
    uint8_t actual_sha256[PD_SHA256_BYTES];

    if (receiver == NULL) {
        return PD_FILE_RECEIVE_INVALID_ARGUMENT;
    }

    if (receiver->state != PD_FILE_RECEIVE_RECEIVING) {
        return PD_FILE_RECEIVE_INVALID_STATE;
    }

    if (receiver->received_bytes != receiver->expected_bytes) {
        return PD_FILE_RECEIVE_INCOMPLETE;
    }

    if (receiver->integrity_configured) {
        if (!receiver->integrity_started ||
            receiver->integrity_ops->finish(receiver->integrity_context, actual_sha256) !=
                PD_INTEGRITY_OK) {
            return pd_file_receiver_fail(receiver, PD_FILE_RECEIVE_INTEGRITY_ERROR);
        }
        receiver->integrity_started = false;

        if (!pd_digest_equal(actual_sha256, receiver->expected_sha256)) {
            if (receiver->sink_started) {
                receiver->sink_ops->abort(receiver->sink_context);
                receiver->sink_started = false;
            }
            receiver->state = PD_FILE_RECEIVE_FAILED;
            return PD_FILE_RECEIVE_INTEGRITY_MISMATCH;
        }
    }

    if (receiver->sink_ops->commit(receiver->sink_context) != PD_FILE_SINK_OK) {
        return pd_file_receiver_fail(receiver, PD_FILE_RECEIVE_SINK_ERROR);
    }

    receiver->sink_started = false;
    receiver->state = PD_FILE_RECEIVE_COMPLETE;
    return PD_FILE_RECEIVE_OK;
}

void pd_file_receiver_abort(pd_file_receiver *receiver)
{
    if (receiver == NULL || receiver->state != PD_FILE_RECEIVE_RECEIVING) {
        return;
    }

    pd_file_receiver_abort_resources(receiver);
    receiver->state = PD_FILE_RECEIVE_ABORTED;
}
