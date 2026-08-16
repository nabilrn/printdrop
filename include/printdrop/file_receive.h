#ifndef PRINTDROP_FILE_RECEIVE_H
#define PRINTDROP_FILE_RECEIVE_H

#include "printdrop/frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum pd_file_sink_status {
    PD_FILE_SINK_OK = 0,
    PD_FILE_SINK_IO_ERROR
} pd_file_sink_status;

typedef struct pd_file_sink_ops {
    pd_file_sink_status (*begin)(void *context, uint64_t expected_bytes);
    pd_file_sink_status (*write)(void *context,
                                 const uint8_t *data,
                                 size_t data_size,
                                 size_t *bytes_written);
    pd_file_sink_status (*commit)(void *context);
    void (*abort)(void *context);
} pd_file_sink_ops;

typedef enum pd_file_receive_state {
    PD_FILE_RECEIVE_IDLE = 0,
    PD_FILE_RECEIVE_RECEIVING,
    PD_FILE_RECEIVE_COMPLETE,
    PD_FILE_RECEIVE_FAILED,
    PD_FILE_RECEIVE_ABORTED
} pd_file_receive_state;

typedef enum pd_file_receive_result {
    PD_FILE_RECEIVE_OK = 0,
    PD_FILE_RECEIVE_INVALID_ARGUMENT,
    PD_FILE_RECEIVE_INVALID_STATE,
    PD_FILE_RECEIVE_CHUNK_TOO_LARGE,
    PD_FILE_RECEIVE_EXCEEDS_EXPECTED_SIZE,
    PD_FILE_RECEIVE_INCOMPLETE,
    PD_FILE_RECEIVE_SINK_ERROR
} pd_file_receive_result;

typedef struct pd_file_receiver {
    const pd_file_sink_ops *sink_ops;
    void *sink_context;
    uint64_t expected_bytes;
    uint64_t received_bytes;
    pd_file_receive_state state;
    bool sink_started;
} pd_file_receiver;

pd_file_receive_result pd_file_receiver_init(pd_file_receiver *receiver,
                                             const pd_file_sink_ops *sink_ops,
                                             void *sink_context,
                                             uint64_t expected_bytes);
pd_file_receive_result pd_file_receiver_begin(pd_file_receiver *receiver);
pd_file_receive_result pd_file_receiver_write(pd_file_receiver *receiver,
                                              const uint8_t *data,
                                              size_t data_size);
pd_file_receive_result pd_file_receiver_finish(pd_file_receiver *receiver);
void pd_file_receiver_abort(pd_file_receiver *receiver);

#endif
