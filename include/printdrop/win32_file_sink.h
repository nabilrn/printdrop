#ifndef PRINTDROP_WIN32_FILE_SINK_H
#define PRINTDROP_WIN32_FILE_SINK_H

#ifdef _WIN32

#include "printdrop/file_receive.h"
#include "printdrop/filename.h"
#include "printdrop/job_id.h"

#include <stdbool.h>
#include <wchar.h>

#define PD_WIN32_PATH_CAPACITY 260U

typedef struct pd_win32_file_sink {
    void *file_handle;
    wchar_t job_directory[PD_WIN32_PATH_CAPACITY];
    wchar_t staging_path[PD_WIN32_PATH_CAPACITY];
    wchar_t final_path[PD_WIN32_PATH_CAPACITY];
    bool initialized;
} pd_win32_file_sink;

typedef enum pd_win32_sink_result {
    PD_WIN32_SINK_OK = 0,
    PD_WIN32_SINK_INVALID_ARGUMENT,
    PD_WIN32_SINK_INVALID_FILENAME,
    PD_WIN32_SINK_INVALID_JOB_ID,
    PD_WIN32_SINK_INVALID_UTF8,
    PD_WIN32_SINK_PATH_TOO_LONG
} pd_win32_sink_result;

pd_win32_sink_result pd_win32_file_sink_init(pd_win32_file_sink *sink,
                                             const wchar_t *root_directory,
                                             const char *job_id,
                                             const char *sanitized_filename);
const pd_file_sink_ops *pd_win32_file_sink_ops(void);

#endif

#endif
