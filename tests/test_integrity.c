#include "printdrop/integrity.h"

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

int main(void)
{
    static const char known_hex[] =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    uint8_t digest[PD_SHA256_BYTES];
    uint8_t copy[PD_SHA256_BYTES];
    char encoded[PD_SHA256_HEX_CAPACITY];

    PD_TEST_ASSERT(pd_sha256_from_hex(known_hex, digest) == PD_DIGEST_OK);
    memcpy(copy, digest, sizeof(copy));
    PD_TEST_ASSERT(pd_digest_equal(digest, copy));
    copy[31] ^= UINT8_C(1);
    PD_TEST_ASSERT(!pd_digest_equal(digest, copy));

    pd_sha256_to_hex(digest, encoded);
    PD_TEST_ASSERT(strcmp(encoded, known_hex) == 0);
    PD_TEST_ASSERT(pd_sha256_from_hex("xyz", digest) == PD_DIGEST_INVALID_HEX);
    PD_TEST_ASSERT(pd_sha256_from_hex(
                       "ga7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                       digest) == PD_DIGEST_INVALID_HEX);

    if (failures != 0) {
        fprintf(stderr, "%d integrity assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop integrity tests passed.");
    return 0;
}
