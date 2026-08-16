#include "printdrop/win32_file_sink.h"

#ifdef _WIN32

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

static bool pd_win32_handle_is_open(const pd_win32_file_sink *sink)
{
    return sink->file_handle != NULL && sink->file_handle != INVALID_HANDLE_VALUE;
}

static bool pd_win32_directory_exists(const wchar_t *path)
{
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
}

static bool pd_win32_ensure_directory(const wchar_t *path)
{
    if (CreateDirectoryW(path, NULL) != 0) {
        return true;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return pd_win32_directory_exists(path);
    }

    return false;
}

static bool pd_win32_append_component(wchar_t *destination,
                                      size_t capacity,
                                      const wchar_t *component)
{
    size_t destination_length = wcslen(destination);
    size_t component_length = wcslen(component);
    bool needs_separator = destination_length > 0U && destination[destination_length - 1U] != L'\\' &&
                           destination[destination_length - 1U] != L'/';
    size_t required = destination_length + (needs_separator ? 1U : 0U) + component_length + 1U;

    if (required > capacity) {
        return false;
    }

    if (needs_separator) {
        destination[destination_length] = L'\\';
        destination_length += 1U;
    }

    memcpy(&destination[destination_length], component, (component_length + 1U) * sizeof(wchar_t));
    return true;
}

static bool pd_win32_copy_path(wchar_t *destination, size_t capacity, const wchar_t *source)
{
    size_t length = wcslen(source);
    if (length + 1U > capacity) {
        return false;
    }
    memcpy(destination, source, (length + 1U) * sizeof(wchar_t));
    return true;
}

static bool pd_win32_job_id_to_wide(const char *job_id, wchar_t output[PD_JOB_ID_CAPACITY])
{
    size_t index;

    if (!pd_job_id_is_valid(job_id)) {
        return false;
    }

    for (index = 0U; index < (size_t)PD_JOB_ID_HEX_CHARS; ++index) {
        output[index] = (wchar_t)(unsigned char)job_id[index];
    }
    output[PD_JOB_ID_HEX_CHARS] = L'\0';
    return true;
}

static pd_file_sink_status pd_win32_sink_begin(void *context, uint64_t expected_bytes)
{
    pd_win32_file_sink *sink = (pd_win32_file_sink *)context;
    HANDLE handle;
    (void)expected_bytes;

    if (sink == NULL || !sink->initialized || pd_win32_handle_is_open(sink)) {
        return PD_FILE_SINK_IO_ERROR;
    }

    if (!pd_win32_ensure_directory(sink->job_directory)) {
        return PD_FILE_SINK_IO_ERROR;
    }

    handle = CreateFileW(sink->staging_path,
                         GENERIC_WRITE,
                         0U,
                         NULL,
                         CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                         NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return PD_FILE_SINK_IO_ERROR;
    }

    sink->file_handle = handle;
    return PD_FILE_SINK_OK;
}

static pd_file_sink_status pd_win32_sink_write(void *context,
                                               const uint8_t *data,
                                               size_t data_size,
                                               size_t *bytes_written)
{
    pd_win32_file_sink *sink = (pd_win32_file_sink *)context;
    DWORD written = 0U;

    if (bytes_written == NULL) {
        return PD_FILE_SINK_IO_ERROR;
    }
    *bytes_written = 0U;

    if (sink == NULL || !pd_win32_handle_is_open(sink) ||
        (data == NULL && data_size != 0U) || data_size > (size_t)DWORD_MAX) {
        return PD_FILE_SINK_IO_ERROR;
    }

    if (WriteFile((HANDLE)sink->file_handle, data, (DWORD)data_size, &written, NULL) == 0) {
        return PD_FILE_SINK_IO_ERROR;
    }

    *bytes_written = (size_t)written;
    return PD_FILE_SINK_OK;
}

static pd_file_sink_status pd_win32_sink_commit(void *context)
{
    pd_win32_file_sink *sink = (pd_win32_file_sink *)context;
    HANDLE handle;

    if (sink == NULL || !pd_win32_handle_is_open(sink)) {
        return PD_FILE_SINK_IO_ERROR;
    }

    handle = (HANDLE)sink->file_handle;
    if (FlushFileBuffers(handle) == 0) {
        return PD_FILE_SINK_IO_ERROR;
    }

    if (CloseHandle(handle) == 0) {
        return PD_FILE_SINK_IO_ERROR;
    }
    sink->file_handle = INVALID_HANDLE_VALUE;

    if (MoveFileExW(sink->staging_path, sink->final_path, MOVEFILE_WRITE_THROUGH) == 0) {
        return PD_FILE_SINK_IO_ERROR;
    }

    return PD_FILE_SINK_OK;
}

static void pd_win32_sink_abort(void *context)
{
    pd_win32_file_sink *sink = (pd_win32_file_sink *)context;

    if (sink == NULL) {
        return;
    }

    if (pd_win32_handle_is_open(sink)) {
        CloseHandle((HANDLE)sink->file_handle);
        sink->file_handle = INVALID_HANDLE_VALUE;
    }

    if (sink->staging_path[0] != L'\0') {
        DeleteFileW(sink->staging_path);
    }
}

pd_win32_sink_result pd_win32_file_sink_init(pd_win32_file_sink *sink,
                                             const wchar_t *root_directory,
                                             const char *job_id,
                                             const char *sanitized_filename)
{
    char checked_filename[PD_FILENAME_CAPACITY];
    wchar_t wide_job_id[PD_JOB_ID_CAPACITY];
    wchar_t wide_filename[PD_FILENAME_CAPACITY];
    wchar_t staging_name[PD_FILENAME_CAPACITY + 6U];
    int wide_length;

    if (sink == NULL || root_directory == NULL || root_directory[0] == L'\0' ||
        job_id == NULL || sanitized_filename == NULL) {
        return PD_WIN32_SINK_INVALID_ARGUMENT;
    }

    memset(sink, 0, sizeof(*sink));
    sink->file_handle = INVALID_HANDLE_VALUE;

    if (!pd_job_id_is_valid(job_id) || !pd_win32_job_id_to_wide(job_id, wide_job_id)) {
        return PD_WIN32_SINK_INVALID_JOB_ID;
    }

    if (pd_filename_sanitize(sanitized_filename,
                             checked_filename,
                             sizeof(checked_filename),
                             NULL) != PD_FILENAME_OK ||
        strcmp(checked_filename, sanitized_filename) != 0) {
        return PD_WIN32_SINK_INVALID_FILENAME;
    }

    wide_length = MultiByteToWideChar(CP_UTF8,
                                      MB_ERR_INVALID_CHARS,
                                      sanitized_filename,
                                      -1,
                                      wide_filename,
                                      (int)PD_FILENAME_CAPACITY);
    if (wide_length == 0) {
        return PD_WIN32_SINK_INVALID_UTF8;
    }

    if (!pd_win32_copy_path(sink->job_directory,
                            (size_t)PD_WIN32_PATH_CAPACITY,
                            root_directory) ||
        !pd_win32_append_component(sink->job_directory,
                                   (size_t)PD_WIN32_PATH_CAPACITY,
                                   wide_job_id)) {
        return PD_WIN32_SINK_PATH_TOO_LONG;
    }

    if (!pd_win32_copy_path(sink->final_path,
                            (size_t)PD_WIN32_PATH_CAPACITY,
                            sink->job_directory) ||
        !pd_win32_append_component(sink->final_path,
                                   (size_t)PD_WIN32_PATH_CAPACITY,
                                   wide_filename)) {
        return PD_WIN32_SINK_PATH_TOO_LONG;
    }

    if (wcslen(wide_filename) + 6U > sizeof(staging_name) / sizeof(staging_name[0])) {
        return PD_WIN32_SINK_PATH_TOO_LONG;
    }
    memcpy(staging_name, wide_filename, (wcslen(wide_filename) + 1U) * sizeof(wchar_t));
    wcscat_s(staging_name, sizeof(staging_name) / sizeof(staging_name[0]), L".part");

    if (!pd_win32_copy_path(sink->staging_path,
                            (size_t)PD_WIN32_PATH_CAPACITY,
                            sink->job_directory) ||
        !pd_win32_append_component(sink->staging_path,
                                   (size_t)PD_WIN32_PATH_CAPACITY,
                                   staging_name)) {
        return PD_WIN32_SINK_PATH_TOO_LONG;
    }

    if (!pd_win32_ensure_directory(root_directory)) {
        return PD_WIN32_SINK_INVALID_ARGUMENT;
    }

    sink->initialized = true;
    return PD_WIN32_SINK_OK;
}

const pd_file_sink_ops *pd_win32_file_sink_ops(void)
{
    static const pd_file_sink_ops ops = {
        pd_win32_sink_begin,
        pd_win32_sink_write,
        pd_win32_sink_commit,
        pd_win32_sink_abort,
    };
    return &ops;
}

#endif
