#include "printdrop/frame.h"

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

static void test_frame_round_trip(void)
{
    pd_frame_header source = {PD_MSG_CHUNK, UINT16_C(0), UINT32_C(0x00012345)};
    pd_frame_header decoded;
    uint8_t bytes[PD_FRAME_HEADER_SIZE];
    const uint8_t expected[PD_FRAME_HEADER_SIZE] = {
        UINT8_C(0x50), UINT8_C(0x44), UINT8_C(0x52), UINT8_C(0x50),
        PD_PROTOCOL_VERSION, (uint8_t)PD_MSG_CHUNK, UINT8_C(0x00), UINT8_C(0x00),
        UINT8_C(0x00), UINT8_C(0x01), UINT8_C(0x23), UINT8_C(0x45),
    };

    PD_TEST_ASSERT(pd_frame_encode_header(&source, bytes) == PD_FRAME_OK);
    PD_TEST_ASSERT(memcmp(bytes, expected, sizeof(expected)) == 0);
    PD_TEST_ASSERT(pd_frame_decode_header(bytes, &decoded) == PD_FRAME_OK);
    PD_TEST_ASSERT(decoded.type == source.type);
    PD_TEST_ASSERT(decoded.flags == source.flags);
    PD_TEST_ASSERT(decoded.payload_length == source.payload_length);
}

static void test_frame_rejects_invalid_boundaries(void)
{
    pd_frame_header header = {PD_MSG_CHUNK, UINT16_C(0), (uint32_t)PD_FRAME_MAX_PAYLOAD};
    uint8_t bytes[PD_FRAME_HEADER_SIZE];

    PD_TEST_ASSERT(pd_frame_encode_header(&header, bytes) == PD_FRAME_OK);

    header.payload_length = (uint32_t)PD_FRAME_MAX_PAYLOAD + UINT32_C(1);
    PD_TEST_ASSERT(pd_frame_encode_header(&header, bytes) == PD_FRAME_PAYLOAD_TOO_LARGE);

    header.payload_length = UINT32_C(1);
    header.flags = UINT16_C(1);
    PD_TEST_ASSERT(pd_frame_encode_header(&header, bytes) == PD_FRAME_UNSUPPORTED_FLAGS);

    header.flags = UINT16_C(0);
    header.type = (pd_message_type)255;
    PD_TEST_ASSERT(pd_frame_encode_header(&header, bytes) == PD_FRAME_INVALID_TYPE);
}

static void test_frame_rejects_malformed_wire_data(void)
{
    pd_frame_header header = {PD_MSG_ACK, UINT16_C(0), UINT32_C(0)};
    pd_frame_header decoded;
    uint8_t bytes[PD_FRAME_HEADER_SIZE];

    PD_TEST_ASSERT(pd_frame_encode_header(&header, bytes) == PD_FRAME_OK);

    bytes[0] = UINT8_C(0x00);
    PD_TEST_ASSERT(pd_frame_decode_header(bytes, &decoded) == PD_FRAME_BAD_MAGIC);
    bytes[0] = UINT8_C(0x50);

    bytes[4] = (uint8_t)(PD_PROTOCOL_VERSION + UINT8_C(1));
    PD_TEST_ASSERT(pd_frame_decode_header(bytes, &decoded) == PD_FRAME_UNSUPPORTED_VERSION);
    bytes[4] = PD_PROTOCOL_VERSION;

    bytes[5] = UINT8_C(255);
    PD_TEST_ASSERT(pd_frame_decode_header(bytes, &decoded) == PD_FRAME_INVALID_TYPE);
    bytes[5] = (uint8_t)PD_MSG_ACK;

    bytes[7] = UINT8_C(1);
    PD_TEST_ASSERT(pd_frame_decode_header(bytes, &decoded) == PD_FRAME_UNSUPPORTED_FLAGS);
    bytes[7] = UINT8_C(0);

    bytes[8] = UINT8_C(0x00);
    bytes[9] = UINT8_C(0x10);
    bytes[10] = UINT8_C(0x00);
    bytes[11] = UINT8_C(0x01);
    PD_TEST_ASSERT(pd_frame_decode_header(bytes, &decoded) == PD_FRAME_PAYLOAD_TOO_LARGE);
}

int main(void)
{
    test_frame_round_trip();
    test_frame_rejects_invalid_boundaries();
    test_frame_rejects_malformed_wire_data();

    if (failures != 0) {
        fprintf(stderr, "%d frame test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop frame tests passed.");
    return 0;
}
