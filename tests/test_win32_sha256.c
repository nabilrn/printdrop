#include "printdrop/integrity.h"
#include "printdrop/win32_sha256.h"

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
    static const uint8_t data[] = {'a', 'b', 'c'};
    static const char expected_hex[] =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    pd_win32_sha256 context;
    uint8_t digest[PD_SHA256_BYTES];
    char hex[PD_SHA256_HEX_CAPACITY];
    const pd_integrity_ops *ops = pd_win32_sha256_ops();

    pd_win32_sha256_init(&context);
    PD_TEST_ASSERT(ops->begin(&context) == PD_INTEGRITY_OK);
    PD_TEST_ASSERT(ops->update(&context, data, 1U) == PD_INTEGRITY_OK);
    PD_TEST_ASSERT(ops->update(&context, &data[1], 2U) == PD_INTEGRITY_OK);
    PD_TEST_ASSERT(ops->finish(&context, digest) == PD_INTEGRITY_OK);
    pd_sha256_to_hex(digest, hex);
    PD_TEST_ASSERT(strcmp(hex, expected_hex) == 0);
    PD_TEST_ASSERT(!context.active);

    if (failures != 0) {
        fprintf(stderr, "%d Win32 SHA-256 assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop Win32 SHA-256 tests passed.");
    return 0;
}
