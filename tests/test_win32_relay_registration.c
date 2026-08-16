#include "printdrop/win32_relay_registration.h"

#ifdef _WIN32

#include <stdio.h>

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
    static const char session[] = "00112233445566778899aabbccddeeff";
    static const char secret[] =
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    char error_text[64];

    PD_TEST_ASSERT(pd_win32_relay_register_session(NULL,
                                                   session,
                                                   secret,
                                                   error_text,
                                                   sizeof(error_text)) ==
                   PD_WIN32_RELAY_REGISTRATION_INVALID_ARGUMENT);
    PD_TEST_ASSERT(pd_win32_relay_register_session("http://relay.printdrop.app",
                                                   session,
                                                   secret,
                                                   error_text,
                                                   sizeof(error_text)) ==
                   PD_WIN32_RELAY_REGISTRATION_INVALID_REQUEST);
    PD_TEST_ASSERT(pd_win32_relay_register_session("https://relay.printdrop.app",
                                                   "short",
                                                   secret,
                                                   error_text,
                                                   sizeof(error_text)) ==
                   PD_WIN32_RELAY_REGISTRATION_INVALID_REQUEST);
    PD_TEST_ASSERT(pd_win32_relay_register_session("https://relay.printdrop.app",
                                                   session,
                                                   "bad",
                                                   error_text,
                                                   sizeof(error_text)) ==
                   PD_WIN32_RELAY_REGISTRATION_INVALID_REQUEST);

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }
    puts("Win32 relay registration validation tests passed.");
    return 0;
}

#endif
