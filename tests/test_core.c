#include "printdrop/protocol.h"
#include "printdrop/session.h"

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

static void test_protocol_message_types(void)
{
    PD_TEST_ASSERT(PD_PROTOCOL_VERSION == UINT8_C(1));
    PD_TEST_ASSERT(pd_message_type_is_valid((uint8_t)PD_MSG_HELLO));
    PD_TEST_ASSERT(pd_message_type_is_valid((uint8_t)PD_MSG_ERROR));
    PD_TEST_ASSERT(!pd_message_type_is_valid(UINT8_C(0)));
    PD_TEST_ASSERT(!pd_message_type_is_valid(UINT8_C(255)));
    PD_TEST_ASSERT(strcmp(pd_message_type_name(PD_MSG_CHUNK), "CHUNK") == 0);
    PD_TEST_ASSERT(strcmp(pd_message_type_name((pd_message_type)255), "UNKNOWN") == 0);
}

static void test_transfer_happy_path(void)
{
    pd_transfer_progress progress;

    pd_transfer_init(&progress, UINT64_C(10));
    PD_TEST_ASSERT(progress.state == PD_TRANSFER_WAITING);
    PD_TEST_ASSERT(progress.received_bytes == UINT64_C(0));

    PD_TEST_ASSERT(pd_transfer_accept_chunk(&progress, UINT64_C(4)) == PD_TRANSFER_OK);
    PD_TEST_ASSERT(progress.state == PD_TRANSFER_RECEIVING);
    PD_TEST_ASSERT(progress.received_bytes == UINT64_C(4));

    PD_TEST_ASSERT(pd_transfer_accept_chunk(&progress, UINT64_C(6)) == PD_TRANSFER_OK);
    PD_TEST_ASSERT(progress.received_bytes == UINT64_C(10));
    PD_TEST_ASSERT(pd_transfer_begin_verification(&progress));
    PD_TEST_ASSERT(progress.state == PD_TRANSFER_VERIFYING);
    PD_TEST_ASSERT(pd_transfer_mark_complete(&progress));
    PD_TEST_ASSERT(progress.state == PD_TRANSFER_COMPLETE);
}

static void test_transfer_rejects_invalid_transitions(void)
{
    pd_transfer_progress progress;

    pd_transfer_init(&progress, UINT64_C(8));
    PD_TEST_ASSERT(pd_transfer_accept_chunk(&progress, UINT64_C(0)) == PD_TRANSFER_INVALID_ARGUMENT);
    PD_TEST_ASSERT(!pd_transfer_begin_verification(&progress));
    PD_TEST_ASSERT(pd_transfer_accept_chunk(&progress, UINT64_C(9)) == PD_TRANSFER_EXCEEDS_EXPECTED_SIZE);
    PD_TEST_ASSERT(progress.received_bytes == UINT64_C(0));

    PD_TEST_ASSERT(pd_transfer_accept_chunk(&progress, UINT64_C(8)) == PD_TRANSFER_OK);
    PD_TEST_ASSERT(pd_transfer_begin_verification(&progress));
    PD_TEST_ASSERT(pd_transfer_accept_chunk(&progress, UINT64_C(1)) == PD_TRANSFER_INVALID_STATE);

    pd_transfer_mark_failed(&progress);
    PD_TEST_ASSERT(progress.state == PD_TRANSFER_FAILED);
    PD_TEST_ASSERT(!pd_transfer_mark_complete(&progress));
}

static void test_zero_length_transfer(void)
{
    pd_transfer_progress progress;

    pd_transfer_init(&progress, UINT64_C(0));
    PD_TEST_ASSERT(pd_transfer_begin_verification(&progress));
    PD_TEST_ASSERT(pd_transfer_mark_complete(&progress));
}

int main(void)
{
    test_protocol_message_types();
    test_transfer_happy_path();
    test_transfer_rejects_invalid_transitions();
    test_zero_length_transfer();

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop core tests passed.");
    return 0;
}
