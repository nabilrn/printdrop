#include "printdrop/file_begin.h"
#include "printdrop/receiver_protocol.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define PD_TEST_ASSERT(condition)                                                                    \
    do {                                                                                             \
        if (!(condition)) {                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                    \
            failures += 1;                                                                           \
        }                                                                                            \
    } while (0)

typedef struct fake_handler {
    unsigned int begin_calls;
    unsigned int chunk_calls;
    unsigned int finish_calls;
    unsigned int abort_calls;
    size_t bytes_received;
    char filename[PD_FILENAME_CAPACITY];
    bool fail_chunk;
} fake_handler;

static bool fake_begin(void *context,
                       const pd_file_begin *metadata,
                       const char *sanitized_filename)
{
    fake_handler *handler = (fake_handler *)context;
    (void)metadata;
    handler->begin_calls += 1U;
    memcpy(handler->filename, sanitized_filename, strlen(sanitized_filename) + 1U);
    return true;
}

static bool fake_chunk(void *context, const uint8_t *data, size_t data_size)
{
    fake_handler *handler = (fake_handler *)context;
    (void)data;
    handler->chunk_calls += 1U;
    if (handler->fail_chunk) {
        return false;
    }
    handler->bytes_received += data_size;
    return true;
}

static bool fake_finish(void *context)
{
    fake_handler *handler = (fake_handler *)context;
    handler->finish_calls += 1U;
    return true;
}

static void fake_abort(void *context)
{
    ((fake_handler *)context)->abort_calls += 1U;
}

static const pd_receiver_protocol_ops fake_ops = {fake_begin, fake_chunk, fake_finish, fake_abort};

static size_t build_file_begin(uint8_t payload[PD_FILE_BEGIN_MAX_PAYLOAD], const char *filename)
{
    pd_file_begin metadata;
    size_t written = 0U;
    size_t index;

    memset(&metadata, 0, sizeof(metadata));
    metadata.file_size = UINT64_C(5);
    for (index = 0U; index < (size_t)PD_SHA256_BYTES; ++index) {
        metadata.sha256[index] = (uint8_t)index;
    }
    memcpy(metadata.filename, filename, strlen(filename) + 1U);
    PD_TEST_ASSERT(pd_file_begin_encode(&metadata,
                                        payload,
                                        (size_t)PD_FILE_BEGIN_MAX_PAYLOAD,
                                        &written) == PD_FILE_BEGIN_OK);
    return written;
}

static void test_happy_path_and_sanitization(void)
{
    fake_handler handler = {0};
    pd_receiver_protocol protocol;
    uint8_t begin_payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    const uint8_t chunk[] = {UINT8_C(1), UINT8_C(2), UINT8_C(3), UINT8_C(4), UINT8_C(5)};
    size_t begin_size = build_file_begin(begin_payload, "../../skripsi.pdf");
    pd_frame_header begin_header = {PD_MSG_FILE_BEGIN, UINT16_C(0), (uint32_t)begin_size};
    pd_frame_header chunk_header = {PD_MSG_CHUNK, UINT16_C(0), (uint32_t)sizeof(chunk)};
    pd_frame_header end_header = {PD_MSG_FILE_END, UINT16_C(0), UINT32_C(0)};

    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol, &fake_ops, &handler) ==
                   PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &begin_header,
                                               begin_payload,
                                               begin_size) == PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_RECEIVING);
    PD_TEST_ASSERT(strcmp(handler.filename, ".._.._skripsi.pdf") == 0);
    PD_TEST_ASSERT(handler.begin_calls == 1U);

    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &chunk_header,
                                               chunk,
                                               sizeof(chunk)) == PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(handler.bytes_received == sizeof(chunk));

    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol, &end_header, NULL, 0U) ==
                   PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_COMPLETE);
    PD_TEST_ASSERT(handler.finish_calls == 1U);
    PD_TEST_ASSERT(handler.abort_calls == 0U);
}

static void test_invalid_order_fails_closed(void)
{
    fake_handler handler = {0};
    pd_receiver_protocol protocol;
    const uint8_t chunk[] = {UINT8_C(1)};
    pd_frame_header chunk_header = {PD_MSG_CHUNK, UINT16_C(0), UINT32_C(1)};

    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol, &fake_ops, &handler) ==
                   PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &chunk_header,
                                               chunk,
                                               sizeof(chunk)) ==
                   PD_RECEIVER_PROTOCOL_UNEXPECTED_MESSAGE);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_FAILED);
    PD_TEST_ASSERT(handler.abort_calls == 0U);
}

static void test_handler_failure_aborts_active_file(void)
{
    fake_handler handler = {0};
    pd_receiver_protocol protocol;
    uint8_t begin_payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    const uint8_t chunk[] = {UINT8_C(7)};
    size_t begin_size = build_file_begin(begin_payload, "safe.pdf");
    pd_frame_header begin_header = {PD_MSG_FILE_BEGIN, UINT16_C(0), (uint32_t)begin_size};
    pd_frame_header chunk_header = {PD_MSG_CHUNK, UINT16_C(0), UINT32_C(1)};

    handler.fail_chunk = true;
    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol, &fake_ops, &handler) ==
                   PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &begin_header,
                                               begin_payload,
                                               begin_size) == PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &chunk_header,
                                               chunk,
                                               sizeof(chunk)) ==
                   PD_RECEIVER_PROTOCOL_HANDLER_ERROR);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_FAILED);
    PD_TEST_ASSERT(handler.abort_calls == 1U);
}

static void test_payload_length_mismatch_is_rejected(void)
{
    fake_handler handler = {0};
    pd_receiver_protocol protocol;
    uint8_t begin_payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    size_t begin_size = build_file_begin(begin_payload, "safe.pdf");
    pd_frame_header begin_header = {PD_MSG_FILE_BEGIN,
                                    UINT16_C(0),
                                    (uint32_t)(begin_size + 1U)};

    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol, &fake_ops, &handler) ==
                   PD_RECEIVER_PROTOCOL_OK);
    PD_TEST_ASSERT(pd_receiver_protocol_handle(&protocol,
                                               &begin_header,
                                               begin_payload,
                                               begin_size) ==
                   PD_RECEIVER_PROTOCOL_MALFORMED_FRAME);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_FAILED);
}

int main(void)
{
    test_happy_path_and_sanitization();
    test_invalid_order_fails_closed();
    test_handler_failure_aborts_active_file();
    test_payload_length_mismatch_is_rejected();

    if (failures != 0) {
        fprintf(stderr, "%d receiver protocol assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop receiver protocol tests passed.");
    return 0;
}
