#include "printdrop/file_begin.h"
#include "printdrop/frame.h"
#include "printdrop/integrity.h"
#include "printdrop/job_id.h"
#include "printdrop/receiver_protocol.h"
#include "printdrop/win32_random.h"
#include "printdrop/win32_receive_handler.h"

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
    return path != NULL && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void make_root(wchar_t root[MAX_PATH], const wchar_t *suffix)
{
    wchar_t temp[MAX_PATH];
    PD_TEST_ASSERT(GetTempPathW(MAX_PATH, temp) != 0U);
    PD_TEST_ASSERT(swprintf_s(root,
                             MAX_PATH,
                             L"%sPrintDrop-Rx-%lu-%s",
                             temp,
                             (unsigned long)GetCurrentProcessId(),
                             suffix) > 0);
    RemoveDirectoryW(root);
}

static size_t build_begin_payload(uint8_t payload[PD_FILE_BEGIN_MAX_PAYLOAD],
                                  const char *filename,
                                  const char *sha256_hex,
                                  uint64_t file_size)
{
    pd_file_begin metadata;
    size_t written = 0U;

    memset(&metadata, 0, sizeof(metadata));
    metadata.file_size = file_size;
    PD_TEST_ASSERT(pd_sha256_from_hex(sha256_hex, metadata.sha256) == PD_DIGEST_OK);
    memcpy(metadata.filename, filename, strlen(filename) + 1U);
    PD_TEST_ASSERT(pd_file_begin_encode(&metadata,
                                        payload,
                                        (size_t)PD_FILE_BEGIN_MAX_PAYLOAD,
                                        &written) == PD_FILE_BEGIN_OK);
    return written;
}

static void cleanup_handler(pd_win32_receive_handler *handler)
{
    DeleteFileW(handler->sink.staging_path);
    DeleteFileW(handler->sink.final_path);
    RemoveDirectoryW(handler->sink.job_directory);
    RemoveDirectoryW(handler->root_directory);
}

static void test_full_frame_to_disk_happy_path(void)
{
    static const char abc_sha256[] =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    static const uint8_t data[] = {'a', 'b', 'c'};
    wchar_t root[MAX_PATH];
    pd_win32_receive_handler handler;
    pd_receiver_protocol protocol;
    uint8_t begin_payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    size_t begin_size;
    pd_frame_header begin_header;
    pd_frame_header chunk_header = {PD_MSG_CHUNK, UINT16_C(0), (uint32_t)sizeof(data)};
    pd_frame_header end_header = {PD_MSG_FILE_END, UINT16_C(0), UINT32_C(0)};
    HANDLE file;
    uint8_t readback[sizeof(data)];
    DWORD read_count = 0U;
    uint8_t random_probe[32];

    make_root(root, L"ok");
    PD_TEST_ASSERT(pd_win32_random_fill(NULL, random_probe, sizeof(random_probe)));
    PD_TEST_ASSERT(pd_win32_receive_handler_init(&handler, root) ==
                   PD_WIN32_RECEIVE_HANDLER_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol,
                                             pd_win32_receive_handler_ops(),
                                             &handler) == PD_RECEIVER_PROTOCOL_OK);

    begin_size = build_begin_payload(begin_payload,
                                     "folder\\skripsi.pdf",
                                     abc_sha256,
                                     UINT64_C(3));
    begin_header.type = PD_MSG_FILE_BEGIN;
    begin_header.flags = UINT16_C(0);
    begin_header.payload_length = (uint32_t)begin_size;

    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &begin_header,
                                               begin_payload,
                                               begin_size) == PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(handler.active);
    PD_TEST_ASSERT(pd_job_id_is_valid(handler.job_id));
    PD_TEST_ASSERT(path_exists(handler.sink.staging_path));
    PD_TEST_ASSERT(!path_exists(handler.sink.final_path));

    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &chunk_header,
                                               data,
                                               sizeof(data)) == PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol, &end_header, NULL, 0U) ==
                   PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_COMPLETE);
    PD_TEST_ASSERT(!handler.active);
    PD_TEST_ASSERT(!path_exists(handler.sink.staging_path));
    PD_TEST_ASSERT(path_exists(pd_win32_receive_handler_final_path(&handler)));

    file = CreateFileW(handler.sink.final_path,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       0U,
                       NULL);
    PD_TEST_ASSERT(file != INVALID_HANDLE_VALUE);
    if (file != INVALID_HANDLE_VALUE) {
        PD_TEST_ASSERT(ReadFile(file,
                                readback,
                                (DWORD)sizeof(readback),
                                &read_count,
                                NULL) != 0);
        PD_TEST_ASSERT(read_count == (DWORD)sizeof(data));
        PD_TEST_ASSERT(memcmp(readback, data, sizeof(data)) == 0);
        CloseHandle(file);
    }

    cleanup_handler(&handler);
}

static void test_digest_mismatch_never_creates_final_file(void)
{
    static const char wrong_sha256[] =
        "0000000000000000000000000000000000000000000000000000000000000000";
    static const uint8_t data[] = {'a', 'b', 'c'};
    wchar_t root[MAX_PATH];
    pd_win32_receive_handler handler;
    pd_receiver_protocol protocol;
    uint8_t begin_payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    size_t begin_size;
    pd_frame_header begin_header;
    pd_frame_header chunk_header = {PD_MSG_CHUNK, UINT16_C(0), (uint32_t)sizeof(data)};
    pd_frame_header end_header = {PD_MSG_FILE_END, UINT16_C(0), UINT32_C(0)};

    make_root(root, L"bad-hash");
    PD_TEST_ASSERT(pd_win32_receive_handler_init(&handler, root) ==
                   PD_WIN32_RECEIVE_HANDLER_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol,
                                             pd_win32_receive_handler_ops(),
                                             &handler) == PD_RECEIVER_PROTOCOL_OK);

    begin_size = build_begin_payload(begin_payload, "bad.pdf", wrong_sha256, UINT64_C(3));
    begin_header.type = PD_MSG_FILE_BEGIN;
    begin_header.flags = UINT16_C(0);
    begin_header.payload_length = (uint32_t)begin_size;

    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &begin_header,
                                               begin_payload,
                                               begin_size) == PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &chunk_header,
                                               data,
                                               sizeof(data)) == PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol, &end_header, NULL, 0U) ==
                   PD_RECEIVER_PROTOCOL_HANDLER_ERROR);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_FAILED);
    PD_TEST_ASSERT(!path_exists(handler.sink.staging_path));
    PD_TEST_ASSERT(!path_exists(handler.sink.final_path));

    cleanup_handler(&handler);
}

int main(void)
{
    test_full_frame_to_disk_happy_path();
    test_digest_mismatch_never_creates_final_file();

    if (failures != 0) {
        fprintf(stderr, "%d Win32 receive integration assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop Win32 receive integration tests passed.");
    return 0;
}
