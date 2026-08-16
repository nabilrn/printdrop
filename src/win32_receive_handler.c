#include "printdrop/win32_receive_handler.h"

#ifdef _WIN32

#include "printdrop/win32_random.h"

#include <string.h>
#include <wchar.h>

pd_win32_receive_handler_result pd_win32_receive_handler_init(
    pd_win32_receive_handler *handler,
    const wchar_t *root_directory)
{
    size_t root_length;

    if (handler == NULL || root_directory == NULL || root_directory[0] == L'\0') {
        return PD_WIN32_RECEIVE_HANDLER_INVALID_ARGUMENT;
    }

    root_length = wcslen(root_directory);
    if (root_length + 1U > (size_t)PD_WIN32_PATH_CAPACITY) {
        return PD_WIN32_RECEIVE_HANDLER_PATH_TOO_LONG;
    }

    memset(handler, 0, sizeof(*handler));
    memcpy(handler->root_directory,
           root_directory,
           (root_length + 1U) * sizeof(wchar_t));
    handler->initialized = true;
    return PD_WIN32_RECEIVE_HANDLER_OK;
}

static bool pd_win32_receive_begin(void *context,
                                   const pd_file_begin *metadata,
                                   const char *sanitized_filename)
{
    pd_win32_receive_handler *handler = (pd_win32_receive_handler *)context;

    if (handler == NULL || metadata == NULL || sanitized_filename == NULL ||
        !handler->initialized || handler->active) {
        return false;
    }

    if (pd_job_id_create(handler->job_id, pd_win32_random_fill, NULL) != PD_JOB_ID_OK) {
        return false;
    }

    if (pd_win32_file_sink_init(&handler->sink,
                                handler->root_directory,
                                handler->job_id,
                                sanitized_filename) != PD_WIN32_SINK_OK) {
        return false;
    }

    pd_win32_sha256_init(&handler->sha256);
    if (pd_file_receiver_init(&handler->receiver,
                              pd_win32_file_sink_ops(),
                              &handler->sink,
                              metadata->file_size) != PD_FILE_RECEIVE_OK) {
        return false;
    }

    if (pd_file_receiver_configure_integrity(&handler->receiver,
                                             pd_win32_sha256_ops(),
                                             &handler->sha256,
                                             metadata->sha256) != PD_FILE_RECEIVE_OK) {
        return false;
    }

    if (pd_file_receiver_begin(&handler->receiver) != PD_FILE_RECEIVE_OK) {
        return false;
    }

    handler->active = true;
    return true;
}

static bool pd_win32_receive_chunk(void *context, const uint8_t *data, size_t data_size)
{
    pd_win32_receive_handler *handler = (pd_win32_receive_handler *)context;

    if (handler == NULL || !handler->active) {
        return false;
    }

    return pd_file_receiver_write(&handler->receiver, data, data_size) == PD_FILE_RECEIVE_OK;
}

static bool pd_win32_receive_finish(void *context)
{
    pd_win32_receive_handler *handler = (pd_win32_receive_handler *)context;
    pd_file_receive_result result;

    if (handler == NULL || !handler->active) {
        return false;
    }

    result = pd_file_receiver_finish(&handler->receiver);
    handler->active = false;
    return result == PD_FILE_RECEIVE_OK;
}

static void pd_win32_receive_abort(void *context)
{
    pd_win32_receive_handler *handler = (pd_win32_receive_handler *)context;

    if (handler == NULL || !handler->active) {
        return;
    }

    pd_file_receiver_abort(&handler->receiver);
    handler->active = false;
}

const pd_receiver_protocol_ops *pd_win32_receive_handler_ops(void)
{
    static const pd_receiver_protocol_ops ops = {
        pd_win32_receive_begin,
        pd_win32_receive_chunk,
        pd_win32_receive_finish,
        pd_win32_receive_abort,
    };
    return &ops;
}

const wchar_t *pd_win32_receive_handler_final_path(const pd_win32_receive_handler *handler)
{
    if (handler == NULL || !handler->initialized || handler->sink.final_path[0] == L'\0') {
        return NULL;
    }
    return handler->sink.final_path;
}

#endif
