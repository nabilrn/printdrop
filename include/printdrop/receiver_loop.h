#ifndef PRINTDROP_RECEIVER_LOOP_H
#define PRINTDROP_RECEIVER_LOOP_H

#include "printdrop/frame.h"
#include "printdrop/receiver_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum pd_receiver_loop_event {
    PD_RECEIVER_LOOP_READY = 0,
    PD_RECEIVER_LOOP_FILE_STARTED,
    PD_RECEIVER_LOOP_PROGRESS,
    PD_RECEIVER_LOOP_VERIFYING,
    PD_RECEIVER_LOOP_COMPLETE,
    PD_RECEIVER_LOOP_FAILED
} pd_receiver_loop_event;

typedef enum pd_receiver_loop_result {
    PD_RECEIVER_LOOP_OK = 0,
    PD_RECEIVER_LOOP_INVALID_ARGUMENT,
    PD_RECEIVER_LOOP_RECEIVE_ERROR,
    PD_RECEIVER_LOOP_PROTOCOL_ERROR,
    PD_RECEIVER_LOOP_SEND_ERROR
} pd_receiver_loop_result;

typedef struct pd_receiver_loop_io_ops {
    bool (*receive_frame)(void *context,
                          pd_frame_header *header,
                          const uint8_t **payload,
                          size_t *payload_size);
    bool (*send_frame)(void *context,
                       const pd_frame_header *header,
                       const uint8_t *payload,
                       size_t payload_size);
} pd_receiver_loop_io_ops;

typedef uint64_t (*pd_receiver_loop_progress_fn)(void *context);
typedef void (*pd_receiver_loop_event_fn)(void *context,
                                          pd_receiver_loop_event event,
                                          uint64_t received_bytes);

typedef struct pd_receiver_loop {
    const pd_receiver_loop_io_ops *io_ops;
    void *io_context;
    pd_receiver_protocol *protocol;
    pd_receiver_loop_progress_fn progress;
    void *progress_context;
    pd_receiver_loop_event_fn event_callback;
    void *event_context;
} pd_receiver_loop;

pd_receiver_loop_result pd_receiver_loop_run(pd_receiver_loop *loop);

#endif
