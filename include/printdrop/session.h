#ifndef PRINTDROP_SESSION_H
#define PRINTDROP_SESSION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum pd_transfer_state {
    PD_TRANSFER_WAITING = 0,
    PD_TRANSFER_RECEIVING,
    PD_TRANSFER_VERIFYING,
    PD_TRANSFER_COMPLETE,
    PD_TRANSFER_FAILED
} pd_transfer_state;

typedef enum pd_transfer_result {
    PD_TRANSFER_OK = 0,
    PD_TRANSFER_INVALID_ARGUMENT,
    PD_TRANSFER_INVALID_STATE,
    PD_TRANSFER_EXCEEDS_EXPECTED_SIZE
} pd_transfer_result;

typedef struct pd_transfer_progress {
    uint64_t expected_bytes;
    uint64_t received_bytes;
    pd_transfer_state state;
} pd_transfer_progress;

void pd_transfer_init(pd_transfer_progress *progress, uint64_t expected_bytes);
pd_transfer_result pd_transfer_accept_chunk(pd_transfer_progress *progress, uint64_t chunk_bytes);
bool pd_transfer_begin_verification(pd_transfer_progress *progress);
bool pd_transfer_mark_complete(pd_transfer_progress *progress);
void pd_transfer_mark_failed(pd_transfer_progress *progress);

#endif
