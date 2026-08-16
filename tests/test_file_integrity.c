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
    unsigned int commits;
    unsigned int aborts;
} fake_sink;

typedef struct fake_hash {
    uint8_t value;
    unsigned int aborts;
} fake_hash;

static pd_file_sink_status sink_begin(void *context, uint64_t expected)
{
    (void)context;
    (void)expected;
    return PD_FILE_SINK_OK;
}

static pd_file_sink_status sink_write(void *context,
                                      const uint8_t *data,
                                      size_t size,
                                      size_t *written)
{
    (void)context;
    (void)data;
    *written = size;
    return PD_FILE_SINK_OK;
}

static pd_file_sink_status sink_commit(void *context)
{
    ((fake_sink *)context)->commits += 1U;
    return PD_FILE_SINK_OK;
}

static void sink_abort(void *context)
{
    ((fake_sink *)context)->aborts += 1U;
}

static pd_integrity_status hash_begin(void *context)
{
    ((fake_hash *)context)->value = UINT8_C(0);
    return PD_INTEGRITY_OK;
}

static pd_integrity_status hash_update(void *context, const uint8_t *data, size_t size)
{
    fake_hash *hash = (fake_hash *)context;
    size_t index;
    for (index = 0U; index < size; ++index) {
        hash->value = (uint8_t)(hash->value ^ data[index]);
    }
    return PD_INTEGRITY_OK;
}

static pd_integrity_status hash_finish(void *context, uint8_t digest[PD_SHA256_BYTES])
{
    fake_hash *hash = (fake_hash *)context;
    memset(digest, 0, PD_SHA256_BYTES);
    digest[0] = hash->value;
    return PD_INTEGRITY_OK;
}

static void hash_abort(void *context)
{
    ((fake_hash *)context)->aborts += 1U;
}

static const pd_file_sink_ops sink_ops = {sink_begin, sink_write, sink_commit, sink_abort};
static const pd_integrity_ops hash_ops = {hash_begin, hash_update, hash_finish, hash_abort};

static void test_matching_digest_commits(void)
{
    fake_sink sink = {0U, 0U};
    fake_hash hash = {UINT8_C(0), 0U};
    pd_file_receiver receiver;
    const uint8_t data[] = {UINT8_C(1), UINT8_C(2), UINT8_C(3)};
    uint8_t expected[PD_SHA256_BYTES] = {UINT8_C(0)};

    expected[0] = UINT8_C(1) ^ UINT8_C(2) ^ UINT8_C(3);
    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &sink_ops, &sink, UINT64_C(3)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_configure_integrity(&receiver,
                                                        &hash_ops,
                                                        &hash,
                                                        expected) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, data, sizeof(data)) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(sink.commits == 1U);
    PD_TEST_ASSERT(sink.aborts == 0U);
}

static void test_mismatch_aborts_before_commit(void)
{
    fake_sink sink = {0U, 0U};
    fake_hash hash = {UINT8_C(0), 0U};
    pd_file_receiver receiver;
    const uint8_t data[] = {UINT8_C(4), UINT8_C(5)};
    uint8_t expected[PD_SHA256_BYTES] = {UINT8_C(0)};

    expected[0] = UINT8_C(99);
    PD_TEST_ASSERT(pd_file_receiver_init(&receiver, &sink_ops, &sink, UINT64_C(2)) ==
                   PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_configure_integrity(&receiver,
                                                        &hash_ops,
                                                        &hash,
                                                        expected) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_begin(&receiver) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_write(&receiver, data, sizeof(data)) == PD_FILE_RECEIVE_OK);
    PD_TEST_ASSERT(pd_file_receiver_finish(&receiver) == PD_FILE_RECEIVE_INTEGRITY_MISMATCH);
    PD_TEST_ASSERT(receiver.state == PD_FILE_RECEIVE_FAILED);
    PD_TEST_ASSERT(sink.commits == 0U);
    PD_TEST_ASSERT(sink.aborts == 1U);
}

int main(void)
{
    test_matching_digest_commits();
    test_mismatch_aborts_before_commit();

    if (failures != 0) {
        fprintf(stderr, "%d file integrity assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop file integrity tests passed.");
    return 0;
}
