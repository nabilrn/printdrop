#include "printdrop/relay_message.h"

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

static void test_relay_message_round_trip(void)
{
    const uint8_t payload[] = {UINT8_C(1), UINT8_C(2), UINT8_C(3)};
    pd_frame_header header = {PD_MSG_CHUNK, UINT16_C(0), (uint32_t)sizeof(payload)};
    pd_relay_message_view view;
    uint8_t message[PD_FRAME_HEADER_SIZE + sizeof(payload)];
    size_t written = 0U;

    PD_TEST_ASSERT(pd_relay_message_encode(&header,
                                           payload,
                                           sizeof(payload),
                                           message,
                                           sizeof(message),
                                           &written) == PD_RELAY_MESSAGE_OK);
    PD_TEST_ASSERT(written == sizeof(message));
    PD_TEST_ASSERT(pd_relay_message_decode(message, written, &view) == PD_RELAY_MESSAGE_OK);
    PD_TEST_ASSERT(view.header.type == PD_MSG_CHUNK);
    PD_TEST_ASSERT(view.payload_size == sizeof(payload));
    PD_TEST_ASSERT(memcmp(view.payload, payload, sizeof(payload)) == 0);
}

static void test_relay_message_enforces_smaller_ws_ceiling(void)
{
    static uint8_t payload[PD_RELAY_MAX_PAYLOAD + 1U];
    static uint8_t output[PD_RELAY_WS_MESSAGE_MAX_SIZE];
    pd_frame_header header = {PD_MSG_CHUNK, UINT16_C(0), (uint32_t)sizeof(payload)};
    pd_relay_message_view view;
    size_t written = 99U;

    PD_TEST_ASSERT((size_t)PD_RELAY_MAX_PAYLOAD < (size_t)PD_FRAME_MAX_PAYLOAD);
    PD_TEST_ASSERT(pd_relay_message_encode(&header,
                                           payload,
                                           sizeof(payload),
                                           output,
                                           sizeof(output),
                                           &written) == PD_RELAY_MESSAGE_PAYLOAD_TOO_LARGE);
    PD_TEST_ASSERT(written == 0U);

    memset(output, 0, sizeof(output));
    PD_TEST_ASSERT(pd_relay_message_decode(output, sizeof(output), &view) ==
                   PD_RELAY_MESSAGE_MALFORMED);
}

static void test_zero_payload_frame(void)
{
    pd_frame_header header = {PD_MSG_FILE_END, UINT16_C(0), UINT32_C(0)};
    pd_relay_message_view view;
    uint8_t message[PD_FRAME_HEADER_SIZE];
    size_t written = 0U;

    PD_TEST_ASSERT(pd_relay_message_encode(&header,
                                           NULL,
                                           0U,
                                           message,
                                           sizeof(message),
                                           &written) == PD_RELAY_MESSAGE_OK);
    PD_TEST_ASSERT(pd_relay_message_decode(message, written, &view) == PD_RELAY_MESSAGE_OK);
    PD_TEST_ASSERT(view.payload == NULL);
    PD_TEST_ASSERT(view.payload_size == 0U);
}

int main(void)
{
    test_relay_message_round_trip();
    test_relay_message_enforces_smaller_ws_ceiling();
    test_zero_payload_frame();

    if (failures != 0) {
        fprintf(stderr, "%d relay message assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop relay message tests passed.");
    return 0;
}
