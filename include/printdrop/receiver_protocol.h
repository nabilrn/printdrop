#ifndef PRINTDROP_RECEIVER_PROTOCOL_H
#define PRINTDROP_RECEIVER_PROTOCOL_H

#include "printdrop/file_begin.h"
#include "printdrop/frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum pd_receiver_protocol_state {
    PD_RECEIVER_PROTOCOL_WAIT_FILE = 0,
    PD_RECEIVER_PROTOCOL_RECEIVING,
    PD_RECEIVER_PROTOCOL_COMPLETE,
    PD_RECEIVER_PROTOCOL_FAILED
} pd_receiver_protocol_state;

typedef enum pd_receiver_protocol_result {
    PD_RECEIVER_PROTOCOL_OK = 0,
    PD_RECEIVER_PROTOCOL_INVALID_ARGUMENT,
    PD_RECEIVER_PROTOCOL_INVALID_STATE,
    PD_RECEIVER_PROTOCOL_MALFORMED_FRAME,
    PD_RECEIVER_PROTOCOL_UNEXPECTED_MESSAGE,
    PD_RECEIVER_PROTOCOL_INVALID_METADATA,
    PD_RECEIVER_PROTOCOL_HANDLER_ERROR
} pd_receiver_protocol_result;

typedef struct pd_receiver_protocol_ops {
    bool (*begin_file)(void *context,
                       const pd_file_begin *metadata,
                       const char *sanitized_filename);
    bool (*write_chunk)(void *context, const uint8_t *data, size_t data_size);
    bool (*finish_file)(void *context);
    void (*abort_file)(void *context);
} pd_receiver_protocol_ops;

typedef struct pd_receiver_protocol {
    const pd_receiver_protocol_ops *ops;
    void *context;
    pd_file_begin current_file;
    char sanitized_filename[PD_FILENAME_CAPACITY];
    pd_receiver_protocol_state state;
    bool file_active;
} pd_receiver_protocol;

pd_receiver_protocol_result pd_receiver_protocol_init(pd_receiver_protocol *protocol,
                                                      const pd_receiver_protocol_ops *ops,
                                                      void *context);
pd_receiver_protocol_result pd_receiver_protocol_handle(pd_receiver_protocol *protocol,
                                                        const pd_frame_header *header,
                                                        const uint8_t *payload,
                                                        size_t payload_size);
void pd_receiver_protocol_abort(pd_receiver_protocol *protocol);

#endif
