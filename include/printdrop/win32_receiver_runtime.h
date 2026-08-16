#ifndef PRINTDROP_WIN32_RECEIVER_RUNTIME_H
#define PRINTDROP_WIN32_RECEIVER_RUNTIME_H

#ifdef _WIN32

#include "printdrop/receiver_loop.h"
#include "printdrop/receiver_session.h"

typedef enum pd_win32_receiver_runtime_result {
    PD_WIN32_RECEIVER_RUNTIME_OK = 0,
    PD_WIN32_RECEIVER_RUNTIME_INVALID_ARGUMENT,
    PD_WIN32_RECEIVER_RUNTIME_ENDPOINT_ERROR,
    PD_WIN32_RECEIVER_RUNTIME_REGISTRATION_ERROR,
    PD_WIN32_RECEIVER_RUNTIME_RELAY_ERROR,
    PD_WIN32_RECEIVER_RUNTIME_STORAGE_ERROR,
    PD_WIN32_RECEIVER_RUNTIME_PROTOCOL_ERROR
} pd_win32_receiver_runtime_result;

pd_win32_receiver_runtime_result pd_win32_receiver_run_session(
    const char *relay_https_base_url,
    pd_receiver_session *session,
    const wchar_t *jobs_root_directory,
    pd_receiver_loop_event_fn event_callback,
    void *event_context);

#endif

#endif
