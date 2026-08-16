#include "printdrop/file_receive.h"

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

typedef struct fake_sink {
    uint8_t bytes[64];
    size_t length;
    unsigned int begin_calls;
    unsigned int write_calls;
    unsigned int commit_calls;
    unsigned int abort_calls;
    bool fail_begin;
    bool fail_write;
    bool partial_write;
    bool fail_commit;
} fake_sink;

static pd_file_sink_status fake_begin(void *context, uint64_t expected_bytes)
{
    fake_sink *sink = (fake_sink *)context;
    (void)expected_bytes;
    sink->begin_calls += 1U;
    return sink->fail_begin ? PD_FILE_SINK_IO_ERROR : PD_FILE_SINK_OK;
}

static pd_file_sink_status fake_write(void *context,
                                      const uint8_t *data,
                                      size_t data_size,
                                      size_t *bytes_written)
{
    fake_sink *sink = (fake_sink *)context;
    size_t accepted = data_size;

    sink->write_calls += 1U;
    if (sink->fail_write) {
        *bytes_written = 0U;
        return PD_FILE_SINK_IO_ERROR;
    }

    if (sink->partial_write && accepted > 0U) {
        accepted -= 1U;
    }

    if (sink->length + accepted > sizeof(sink->bytes)) {
        *bytes_written = 0U;
        return PD_FILE_SINK_IO_ERROR;
    }

    memcpy(&sink->bytes[sink->length], data, accepted);
    sink->length += accepted;
    *bytes_written = accepted;
    return PD_FILE_SINK_OK;
}

static pd_file_sink_status fake_commit(void *context)
{
    fake_sink *sink = (fake_sink *)context;
    sink->commit_calls += 1U;
    return sink->fail_commit ? PD_FILE_SINK_IO_ERROR : PD_FILE_SINK_OK;
}

static void fake_abort(void *context)
{
    fake_sink *sink = (fake_sink *)context;
    sink->abort_calls += 1U;
}

static const pd_file_sink_ops fake_ops = {fake_begin, fake_write, fake_commit, fake_abort};

static void test_stream_happy_path(void)
{
    fake_sink sink = {0};
    pd_file_receiver receiver;
    const uint8_t first[] = {UINT8_C(1), UINT8_C(2), UINT8_C(3)};
    const uint8_t second[] = {UINT8_C(4), UINT8_C(5)};

    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &fake_ops, &sink, UINT64_C(5)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, first, sizeof(first)) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, second, sizeof(second)) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(receiver.received_bytes == UINT64_C(5));
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(receiver.state == PD_FILE_RECEIVE_COMPLETE);
    PD_TEST_ASSERT(sink.begin_calls == 1U);
    PD_TEST_ASSERT(sink.write_calls == 2U);
    PD_TEST_ASSERT(sink.commit_calls == 1U);
    PD_TEST_ASSERT(sink.abort_calls == 0U);
    PD_TEST_ASSERT(sink.length == 5U);
}

static void test_stream_enforces_size_boundaries(void)
{
    fake_sink sink = {0};
    pd_file_receiver receiver;
    uint8_t byte = UINT8_C(1);
    static uint8_t oversized[PD_FRAME_MAX_PAYLOAD + 1U];

    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &fake_ops, &sink, UINT64_C(1)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, oversized, sizeof(oversized)) ==
                   PD_FILE_RECEIVE_CHUNK_TOO_LARGE);
    PD_TEST_ASSERT(sink.write_calls == 0U);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, &byte, 1U) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, &byte, 1U) ==
                   PD_FILE_RECEIVE_EXCEEDS_EXPECTED_SIZE);
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_OK);
}

static void test_incomplete_stream_cannot_commit(void)
{
    fake_sink sink = {0};
    pd_file_receiver receiver;
    uint8_t byte = UINT8_C(9);

    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &fake_ops, &sink, UINT64_C(2)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, &byte, 1U) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_INCOMPLETE);
    PD_TEST_ASSERT(sink.commit_calls == 0U);
    PD_TEST_ASSERT(receiver.state == PD_FILE_RECEIVE_RECEIVING);
    pd_file_receiver_abort(&receiver);
    PD_TEST_ASSERT(receiver.state == PD_FILE_RECEIVE_ABORTED);
    PD_TEST_ASSERT(sink.abort_calls == 1U);
}

static void test_sink_failures_abort_staging(void)
{
    fake_sink sink = {0};
    pd_file_receiver receiver;
    uint8_t byte = UINT8_C(3);

    sink.partial_write = true;
    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &fake_ops, &sink, UINT64_C(1)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, &byte, 1U) == PD_FILE_RECEIVE_SINK_ERROR);
    PD_TEST_ASSERT(receiver.state == PD_FILE_RECEIVE_FAILED);
    PD_TEST_ASSERT(sink.abort_calls == 1U);

    memset(&sink, 0, sizeof(sink));
    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &fake_ops, &sink, UINT64_C(0)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    sink.fail_commit = true;
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_SINK_ERROR);
    PD_TEST_ASSERT(receiver.state == PD_FILE_RECEIVE_FAILED);
    PD_TEST_ASSERT(sink.commit_calls == 1U);
    PD_TEST_ASSERT(sink.abort_calls == 1U);
}

static void test_zero_length_file_commits_without_writes(void)
{
    fake_sink sink = {0};
    pd_file_receiver receiver;

    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &fake_ops, &sink, UINT64_C(0)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(sink.write_calls == 0U);
    PD_TEST_ASSERT(sink.commit_calls == 1U);
}

int main(void)
{
    test_stream_happy_path();
    test_stream_enforces_size_boundaries();
    test_incomplete_stream_cannot_commit();
    test_sink_failures_abort_staging();
    test_zero_length_file_commits_without_writes();

    if (failures != 0) {
        fprintf(stderr, "%d file receive test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop file receive tests passed.");
    return 0;
}
