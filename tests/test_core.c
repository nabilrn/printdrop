#include "printdrop/protocol.h"
#include "printdrop/receiver_session.h"
#include "printdrop/session.h"
#include "printdrop/transport.h"

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

typedef struct fake_transport_context {
    unsigned int open_calls;
    unsigned int close_calls;
    uint8_t read_value;
} fake_transport_context;

static pd_transport_status fake_transport_open(void *context)
{
    fake_transport_context *fake = (fake_transport_context *)context;
    fake->open_calls += 1U;
    return PD_TRANSPORT_OK;
}

static pd_transport_status fake_transport_write(void *context,
                                                const uint8_t *buffer,
                                                size_t buffer_size,
                                                size_t *bytes_written)
{
    fake_transport_context *fake = (fake_transport_context *)context;
    (void)buffer;
    fake->read_value = (uint8_t)buffer_size;
    *bytes_written = buffer_size;
    return PD_TRANSPORT_OK;
}

static pd_transport_status fake_transport_read(void *context,
                                               uint8_t *buffer,
                                               size_t buffer_size,
                                               size_t *bytes_read)
{
    fake_transport_context *fake = (fake_transport_context *)context;

    if (buffer_size == 0U) {
        *bytes_read = 0U;
        return PD_TRANSPORT_OK;
    }

    buffer[0] = fake->read_value;
    *bytes_read = 1U;
    return PD_TRANSPORT_OK;
}

static void fake_transport_close(void *context)
{
    fake_transport_context *fake = (fake_transport_context *)context;
    fake->close_calls += 1U;
}

static bool fake_random_fill(void *context, uint8_t *buffer, size_t buffer_size)
{
    size_t index;
    uint8_t seed = context == NULL ? UINT8_C(0) : *(const uint8_t *)context;

    for (index = 0U; index < buffer_size; ++index) {
        buffer[index] = (uint8_t)(seed + (uint8_t)index);
    }

    return true;
}

static bool fake_random_fail(void *context, uint8_t *buffer, size_t buffer_size)
{
    (void)context;
    (void)buffer;
    (void)buffer_size;
    return false;
}

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

static void test_transport_contract(void)
{
    static const pd_transport_ops ops = {
        fake_transport_open,
        fake_transport_write,
        fake_transport_read,
        fake_transport_close,
    };
    fake_transport_context fake = {0U, 0U, UINT8_C(0)};
    pd_transport transport = {&ops, &fake, false};
    const uint8_t payload[] = {UINT8_C(1), UINT8_C(2), UINT8_C(3)};
    uint8_t response = UINT8_C(0);
    size_t transferred = 99U;

    PD_TEST_ASSERT(pd_transport_is_configured(&transport));
    PD_TEST_ASSERT(pd_transport_write(&transport, payload, sizeof(payload), &transferred) ==
                   PD_TRANSPORT_INVALID_STATE);
    PD_TEST_ASSERT(transferred == 0U);

    PD_TEST_ASSERT(pd_transport_open(&transport) == PD_TRANSPORT_OK);
    PD_TEST_ASSERT(transport.is_open);
    PD_TEST_ASSERT(fake.open_calls == 1U);
    PD_TEST_ASSERT(pd_transport_open(&transport) == PD_TRANSPORT_INVALID_STATE);

    PD_TEST_ASSERT(pd_transport_write(&transport, payload, sizeof(payload), &transferred) ==
                   PD_TRANSPORT_OK);
    PD_TEST_ASSERT(transferred == sizeof(payload));
    PD_TEST_ASSERT(pd_transport_read(&transport, &response, sizeof(response), &transferred) ==
                   PD_TRANSPORT_OK);
    PD_TEST_ASSERT(transferred == 1U);
    PD_TEST_ASSERT(response == (uint8_t)sizeof(payload));

    pd_transport_close(&transport);
    PD_TEST_ASSERT(!transport.is_open);
    PD_TEST_ASSERT(fake.close_calls == 1U);
    pd_transport_close(&transport);
    PD_TEST_ASSERT(fake.close_calls == 1U);
}

static void test_receiver_session_lifecycle(void)
{
    pd_receiver_session session;
    uint8_t seed = UINT8_C(0);

    PD_TEST_ASSERT(pd_receiver_session_create(&session,
                                              UINT64_C(1000),
                                              UINT64_C(5000),
                                              fake_random_fill,
                                              &seed) == PD_RECEIVER_SESSION_OK);
    PD_TEST_ASSERT(session.state == PD_RECEIVER_SESSION_WAITING);
    PD_TEST_ASSERT(session.created_at_ms == UINT64_C(1000));
    PD_TEST_ASSERT(session.expires_at_ms == UINT64_C(6000));
    PD_TEST_ASSERT(strcmp(session.token, "000102030405060708090a0b0c0d0e0f") == 0);
    PD_TEST_ASSERT(pd_receiver_session_token_matches(&session, session.token));
    PD_TEST_ASSERT(!pd_receiver_session_token_matches(&session,
                                                       "000102030405060708090a0b0c0d0e00"));

    PD_TEST_ASSERT(pd_receiver_session_begin_transfer(&session,
                                                      "000102030405060708090a0b0c0d0e00",
                                                      UINT64_C(2000)) ==
                   PD_RECEIVER_SESSION_TOKEN_MISMATCH);
    PD_TEST_ASSERT(session.state == PD_RECEIVER_SESSION_WAITING);

    PD_TEST_ASSERT(pd_receiver_session_begin_transfer(&session,
                                                      session.token,
                                                      UINT64_C(2000)) == PD_RECEIVER_SESSION_OK);
    PD_TEST_ASSERT(session.state == PD_RECEIVER_SESSION_TRANSFERRING);

    pd_receiver_session_close(&session);
    PD_TEST_ASSERT(session.state == PD_RECEIVER_SESSION_CLOSED);
    PD_TEST_ASSERT(session.token[0] == '\0');
}

static void test_receiver_session_expiry_and_failures(void)
{
    pd_receiver_session session;
    uint8_t seed = UINT8_C(32);

    PD_TEST_ASSERT(pd_receiver_session_create(&session,
                                              UINT64_MAX - UINT64_C(2),
                                              UINT64_C(3),
                                              fake_random_fill,
                                              &seed) == PD_RECEIVER_SESSION_CLOCK_OVERFLOW);
    PD_TEST_ASSERT(session.state == PD_RECEIVER_SESSION_INACTIVE);

    PD_TEST_ASSERT(pd_receiver_session_create(&session,
                                              UINT64_C(10),
                                              UINT64_C(100),
                                              fake_random_fail,
                                              NULL) == PD_RECEIVER_SESSION_ENTROPY_FAILURE);
    PD_TEST_ASSERT(session.state == PD_RECEIVER_SESSION_INACTIVE);

    PD_TEST_ASSERT(pd_receiver_session_create(&session,
                                              UINT64_C(100),
                                              UINT64_C(50),
                                              fake_random_fill,
                                              &seed) == PD_RECEIVER_SESSION_OK);
    PD_TEST_ASSERT(!pd_receiver_session_expire_if_due(&session, UINT64_C(149)));
    PD_TEST_ASSERT(pd_receiver_session_expire_if_due(&session, UINT64_C(150)));
    PD_TEST_ASSERT(session.state == PD_RECEIVER_SESSION_EXPIRED);
    PD_TEST_ASSERT(pd_receiver_session_begin_transfer(&session,
                                                      session.token,
                                                      UINT64_C(150)) ==
                   PD_RECEIVER_SESSION_EXPIRED_RESULT);
}

int main(void)
{
    test_protocol_message_types();
    test_transfer_happy_path();
    test_transfer_rejects_invalid_transitions();
    test_zero_length_transfer();
    test_transport_contract();
    test_receiver_session_lifecycle();
    test_receiver_session_expiry_and_failures();

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop core tests passed.");
    return 0;
}
