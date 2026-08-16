#include "printdrop/file_begin.h"

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

static void fill_digest(uint8_t digest[PD_SHA256_BYTES])
{
    size_t index;
    for (index = 0U; index < (size_t)PD_SHA256_BYTES; ++index) {
        digest[index] = (uint8_t)index;
    }
}

static void test_file_begin_round_trip(void)
{
    pd_file_begin source;
    pd_file_begin decoded;
    uint8_t payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    size_t written = 0U;

    memset(&source, 0, sizeof(source));
    source.file_size = UINT64_C(0x0102030405060708);
    fill_digest(source.sha256);
    strcpy(source.filename, "skripsi-final.pdf");

    PD_TEST_ASSERT(pd_file_begin_encode(&source, payload, sizeof(payload), &written) ==
                   PD_FILE_BEGIN_OK);
    PD_TEST_ASSERT(written == (size_t)PD_FILE_BEGIN_FIXED_SIZE + strlen(source.filename));
    PD_TEST_ASSERT(payload[0] == UINT8_C(0x01));
    PD_TEST_ASSERT(payload[7] == UINT8_C(0x08));
    PD_TEST_ASSERT(payload[40] == UINT8_C(0x00));
    PD_TEST_ASSERT(payload[41] == (uint8_t)strlen(source.filename));

    PD_TEST_ASSERT(pd_file_begin_decode(payload, written, &decoded) == PD_FILE_BEGIN_OK);
    PD_TEST_ASSERT(decoded.file_size == source.file_size);
    PD_TEST_ASSERT(memcmp(decoded.sha256, source.sha256, PD_SHA256_BYTES) == 0);
    PD_TEST_ASSERT(strcmp(decoded.filename, source.filename) == 0);
}

static void test_file_begin_rejects_bad_sizes(void)
{
    pd_file_begin metadata;
    uint8_t payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    size_t written = 77U;

    memset(&metadata, 0, sizeof(metadata));
    fill_digest(metadata.sha256);
    PD_TEST_ASSERT(pd_file_begin_encode(&metadata, payload, sizeof(payload), &written) ==
                   PD_FILE_BEGIN_INVALID_FILENAME);
    PD_TEST_ASSERT(written == 0U);

    strcpy(metadata.filename, "a.pdf");
    PD_TEST_ASSERT(pd_file_begin_encode(&metadata,
                                        payload,
                                        (size_t)PD_FILE_BEGIN_FIXED_SIZE,
                                        &written) == PD_FILE_BEGIN_BUFFER_TOO_SMALL);
    PD_TEST_ASSERT(written == 0U);

    PD_TEST_ASSERT(pd_file_begin_encode(&metadata, payload, sizeof(payload), &written) ==
                   PD_FILE_BEGIN_OK);
    PD_TEST_ASSERT(pd_file_begin_decode(payload, written - 1U, &metadata) ==
                   PD_FILE_BEGIN_MALFORMED);
    PD_TEST_ASSERT(pd_file_begin_decode(payload,
                                        (size_t)PD_FILE_BEGIN_FIXED_SIZE - 1U,
                                        &metadata) == PD_FILE_BEGIN_MALFORMED);
}

static void test_file_begin_rejects_embedded_nul_filename(void)
{
    pd_file_begin metadata;
    uint8_t payload[PD_FILE_BEGIN_FIXED_SIZE + 5U];

    memset(payload, 0, sizeof(payload));
    payload[41] = UINT8_C(5);
    payload[PD_FILE_BEGIN_FIXED_SIZE + 0U] = (uint8_t)'a';
    payload[PD_FILE_BEGIN_FIXED_SIZE + 1U] = (uint8_t)'b';
    payload[PD_FILE_BEGIN_FIXED_SIZE + 2U] = UINT8_C(0);
    payload[PD_FILE_BEGIN_FIXED_SIZE + 3U] = (uint8_t)'c';
    payload[PD_FILE_BEGIN_FIXED_SIZE + 4U] = (uint8_t)'d';

    PD_TEST_ASSERT(pd_file_begin_decode(payload, sizeof(payload), &metadata) ==
                   PD_FILE_BEGIN_MALFORMED);
}

int main(void)
{
    test_file_begin_round_trip();
    test_file_begin_rejects_bad_sizes();
    test_file_begin_rejects_embedded_nul_filename();

    if (failures != 0) {
        fprintf(stderr, "%d FILE_BEGIN assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop FILE_BEGIN tests passed.");
    return 0;
}
