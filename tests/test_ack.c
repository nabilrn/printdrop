#include "printdrop/ack.h"

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

static void test_ack_round_trip(void)
{
    pd_ack source = {PD_MSG_CHUNK, PD_ACK_OK, UINT64_C(0x0102030405060708)};
    pd_ack decoded;
    uint8_t payload[PD_ACK_PAYLOAD_SIZE];
    const uint8_t expected[PD_ACK_PAYLOAD_SIZE] = {
        (uint8_t)PD_MSG_CHUNK,
        (uint8_t)PD_ACK_OK,
        UINT8_C(0),
        UINT8_C(0),
        UINT8_C(0x01),
        UINT8_C(0x02),
        UINT8_C(0x03),
        UINT8_C(0x04),
        UINT8_C(0x05),
        UINT8_C(0x06),
        UINT8_C(0x07),
        UINT8_C(0x08),
    };

    PD_TEST_ASSERT(pd_ack_encode(&source, payload) == PD_ACK_RESULT_OK);
    PD_TEST_ASSERT(memcmp(payload, expected, sizeof(expected)) == 0);
    PD_TEST_ASSERT(pd_ack_decode(payload, sizeof(payload), &decoded) == PD_ACK_RESULT_OK);
    PD_TEST_ASSERT(decoded.acknowledged_type == source.acknowledged_type);
    PD_TEST_ASSERT(decoded.status == source.status);
    PD_TEST_ASSERT(decoded.received_bytes == source.received_bytes);
}

static void test_ack_rejects_invalid_values(void)
{
    pd_ack ack = {PD_MSG_ACK, PD_ACK_OK, UINT64_C(0)};
    uint8_t payload[PD_ACK_PAYLOAD_SIZE] = {0};

    PD_TEST_ASSERT(pd_ack_encode(&ack, payload) == PD_ACK_INVALID_TYPE);

    ack.acknowledged_type = PD_MSG_FILE_END;
    ack.status = (pd_ack_status)255;
    PD_TEST_ASSERT(pd_ack_encode(&ack, payload) == PD_ACK_INVALID_STATUS);

    payload[0] = (uint8_t)PD_MSG_CHUNK;
    payload[1] = (uint8_t)PD_ACK_OK;
    payload[2] = UINT8_C(1);
    PD_TEST_ASSERT(pd_ack_decode(payload, sizeof(payload), &ack) == PD_ACK_MALFORMED);
    payload[2] = UINT8_C(0);
    PD_TEST_ASSERT(pd_ack_decode(payload, sizeof(payload) - 1U, &ack) == PD_ACK_MALFORMED);
}

int main(void)
{
    test_ack_round_trip();
    test_ack_rejects_invalid_values();

    if (failures != 0) {
        fprintf(stderr, "%d ACK assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop ACK tests passed.");
    return 0;
}
