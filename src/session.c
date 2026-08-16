#include "printdrop/session.h"

void pd_transfer_init(pd_transfer_progress *progress, uint64_t expected_bytes)
{
    if (progress == NULL) {
        return;
    }

    progress->expected_bytes = expected_bytes;
    progress->received_bytes = UINT64_C(0);
    progress->state = PD_TRANSFER_WAITING;
}

pd_transfer_result pd_transfer_accept_chunk(pd_transfer_progress *progress, uint64_t chunk_bytes)
{
    if (progress == NULL || chunk_bytes == UINT64_C(0)) {
        return PD_TRANSFER_INVALID_ARGUMENT;
    }

    if (progress->state != PD_TRANSFER_WAITING && progress->state != PD_TRANSFER_RECEIVING) {
        return PD_TRANSFER_INVALID_STATE;
    }

    if (progress->received_bytes > progress->expected_bytes ||
        chunk_bytes > progress->expected_bytes - progress->received_bytes) {
        return PD_TRANSFER_EXCEEDS_EXPECTED_SIZE;
    }

    progress->state = PD_TRANSFER_RECEIVING;
    progress->received_bytes += chunk_bytes;
    return PD_TRANSFER_OK;
}

bool pd_transfer_begin_verification(pd_transfer_progress *progress)
{
    if (progress == NULL) {
        return false;
    }

    if (progress->received_bytes != progress->expected_bytes) {
        return false;
    }

    if (progress->state != PD_TRANSFER_WAITING && progress->state != PD_TRANSFER_RECEIVING) {
        return false;
    }

    progress->state = PD_TRANSFER_VERIFYING;
    return true;
}

bool pd_transfer_mark_complete(pd_transfer_progress *progress)
{
    if (progress == NULL || progress->state != PD_TRANSFER_VERIFYING) {
        return false;
    }

    progress->state = PD_TRANSFER_COMPLETE;
    return true;
}

void pd_transfer_mark_failed(pd_transfer_progress *progress)
{
    if (progress == NULL || progress->state == PD_TRANSFER_COMPLETE) {
        return;
    }

    progress->state = PD_TRANSFER_FAILED;
}
