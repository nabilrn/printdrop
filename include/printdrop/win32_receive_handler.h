#ifndef PRINTDROP_WIN32_RECEIVE_HANDLER_H
#define PRINTDROP_WIN32_RECEIVE_HANDLER_H

#ifdef _WIN32

#include "printdrop/file_receive.h"
#include "printdrop/job_id.h"
#include "printdrop/receiver_protocol.h"
#include "printdrop/win32_file_sink.h"
#include "printdrop/win32_sha256.h"

#include <stdbool.h>
#include <wchar.h>

typedef struct pd_win32_receive_handler {
    wchar_t root_directory[PD_WIN32_PATH_CAPACITY];
    char job_id[PD_JOB_ID_CAPACITY];
    pd_win32_file_sink sink;
    pd_win32_sha256 sha256;
    pd_file_receiver receiver;
    bool initialized;
    bool active;
} pd_win32_receive_handler;

typedef enum pd_win32_receive_handler_result {
    PD_WIN32_RECEIVE_HANDLER_OK = 0,
    PD_WIN32_RECEIVE_HANDLER_INVALID_ARGUMENT,
    PD_WIN32_RECEIVE_HANDLER_PATH_TOO_LONG
} pd_win32_receive_handler_result;

pd_win32_receive_handler_result pd_win32_receive_handler_init(
    pd_win32_receive_handler *handler,
    const wchar_t *root_directory);
const pd_receiver_protocol_ops *pd_win32_receive_handler_ops(void);
const wchar_t *pd_win32_receive_handler_final_path(const pd_win32_receive_handler *handler);

#endif

#endif
