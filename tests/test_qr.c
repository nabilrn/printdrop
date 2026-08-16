#include "printdrop/qr.h"

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

static void test_qr_encodes_session_url(void)
{
    pd_qr_code code;
    int x;
    int y;
    int dark_modules = 0;

    PD_TEST_ASSERT(pd_qr_encode_text(
                       &code,
                       "https://send.printdrop.app/s/000102030405060708090a0b0c0d0e0f") == PD_QR_OK);
    PD_TEST_ASSERT(pd_qr_size(&code) >= 21);
    PD_TEST_ASSERT(pd_qr_size(&code) <= (PD_QR_MAX_VERSION * 4) + 17);

    for (y = 0; y < pd_qr_size(&code); ++y) {
        for (x = 0; x < pd_qr_size(&code); ++x) {
            if (pd_qr_get_module(&code, x, y)) {
                dark_modules += 1;
            }
        }
    }

    PD_TEST_ASSERT(dark_modules > 0);
    PD_TEST_ASSERT(pd_qr_get_module(&code, -1, 0) == false);
    PD_TEST_ASSERT(pd_qr_get_module(&code, pd_qr_size(&code), 0) == false);
}

static void test_qr_rejects_invalid_and_oversized_input(void)
{
    pd_qr_code code;
    char oversized[1024];

    memset(oversized, 'A', sizeof(oversized) - 1U);
    oversized[sizeof(oversized) - 1U] = '\0';

    PD_TEST_ASSERT(pd_qr_encode_text(NULL, "hello") == PD_QR_INVALID_ARGUMENT);
    PD_TEST_ASSERT(pd_qr_encode_text(&code, NULL) == PD_QR_INVALID_ARGUMENT);
    PD_TEST_ASSERT(pd_qr_encode_text(&code, "") == PD_QR_INVALID_ARGUMENT);
    PD_TEST_ASSERT(pd_qr_encode_text(&code, oversized) == PD_QR_DATA_TOO_LONG);
}

int main(void)
{
    test_qr_encodes_session_url();
    test_qr_rejects_invalid_and_oversized_input();

    if (failures != 0) {
        fprintf(stderr, "%d QR test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop QR tests passed.");
    return 0;
}
