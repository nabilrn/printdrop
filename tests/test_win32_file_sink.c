#include "printdrop/file_receive.h"
#include "printdrop/win32_file_sink.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures = 0;

#define PD_TEST_ASSERT(condition)                                                                    \
    do {                                                                                             \
        if (!(condition)) {                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                    \
            failures += 1;                                                                           \
        }                                                                                            \
    } while (0)

static bool path_exists(const wchar_t *path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void cleanup_sink_paths(const pd_win32_file_sink *sink)
{
    DeleteFileW(sink->staging_path);
    DeleteFileW(sink->final_path);
    RemoveDirectoryW(sink->job_directory);
}

int main(void)
{
    wchar_t temp[MAX_PATH];
    wchar_t root[MAX_PATH];
    pd_win32_file_sink sink;
    pd_file_receiver receiver;
    const char *job_id = "00112233445566778899aabbccddeeff";
    const uint8_t payload[] = {UINT8_C(1), UINT8_C(2), UINT8_C(3), UINT8_C(4)};
    HANDLE file;
    uint8_t readback[sizeof(payload)];
    DWORD read_count = 0U;

    PD_TEST_ASSERT(GetTempPathW(MAX_PATH, temp) != 0U);
    PD_TEST_ASSERT(swprintf_s(root,
                             MAX_PATH,
                             L"%sPrintDrop-%lu",
                             temp,
                             (unsigned long)GetCurrentProcessId()) > 0);
    RemoveDirectoryW(root);

    PD_TEST_ASSERT(pd_win32_file_sink_init(&sink, root, job_id, "skripsi.pdf") == PD_WIN32_SINK_OK);
    PD_TEST_ASSERT(pd_file_receiver_init(&receiver,
                                         pd_win32_file_sink_ops(),
                                         &sink,
                                         (uint64_t)sizeof(payload)) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(path_exists(sink.staging_path));
    PD_TEST_ASSERT(!path_exists(sink.final_path));
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, payload, sizeof(payload)) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(!path_exists(sink.staging_path));
    PD_TEST_ASSERT(path_exists(sink.final_path));

    file = CreateFileW(sink.final_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0U, NULL);
    PD_TEST_ASSERT(file != INVALID_HANDLE_VALUE);
    if (file != INVALID_HANDLE_VALUE) {
        PD_TEST_ASSERT(ReadFile(file, readback, (DWORD)sizeof(readback), &read_count, NULL) != 0);
        PD_TEST_ASSERT(read_count == (DWORD)sizeof(payload));
        PD_TEST_ASSERT(memcmp(readback, payload, sizeof(payload)) == 0);
        CloseHandle(file);
    }

    {
        pd_win32_file_sink collision;
        pd_file_receiver collision_receiver;
        PD_TEST_ASSERT(pd_win32_file_sink_init(&collision, root, job_id, "skripsi.pdf") ==
                       PD_WIN32_SINK_OK);
        PD_TEST_ASSERT(pd_file_receiver_init(&collision_receiver,
                                             pd_win32_file_sink_ops(),
                                             &collision,
                                             UINT64_C(1)) == PD_FILE_RECEIVE_OK);
        PD_TEST_ASSERT(pd_file_receiver_begin(&collision_receiver) == PD_FILE_RECEIVE_OK);
        PD_TEST_ASSERT(pd_file_receiver_write(&collision_receiver, payload, 1U) ==
                       PD_FILE_RECEIVE_OK);
        PD_TEST_ASSERT(pd_file_receiver_finish(&collision_receiver) == PD_FILE_RECEIVE_SINK_ERROR);
        PD_TEST_ASSERT(!path_exists(collision.staging_path));
        PD_TEST_ASSERT(path_exists(collision.final_path));
    }

    cleanup_sink_paths(&sink);
    RemoveDirectoryW(root);

    {
        const char *abort_job = "ffeeddccbbaa99887766554433221100";
        pd_win32_file_sink aborted;
        pd_file_receiver aborted_receiver;
        PD_TEST_ASSERT(pd_win32_file_sink_init(&aborted, root, abort_job, "cancelled.pdf") ==
                       PD_WIN32_SINK_OK);
        PD_TEST_ASSERT(pd_file_receiver_init(&aborted_receiver,
                                             pd_win32_file_sink_ops(),
                                             &aborted,
                                             UINT64_C(10)) == PD_FILE_RECEIVE_OK);
        PD_TEST_ASSERT(pd_file_receiver_begin(&aborted_receiver) == PD_FILE_RECEIVE_OK);
        PD_TEST_ASSERT(pd_file_receiver_write(&aborted_receiver, payload, sizeof(payload)) ==
                       PD_FILE_RECEIVE_OK);
        pd_file_receiver_abort(&aborted_receiver);
        PD_TEST_ASSERT(!path_exists(aborted.staging_path));
        PD_TEST_ASSERT(!path_exists(aborted.final_path));
        cleanup_sink_paths(&aborted);
        RemoveDirectoryW(root);
    }

    if (failures != 0) {
        fprintf(stderr, "%d Win32 sink assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop Win32 file sink tests passed.");
    return 0;
}
