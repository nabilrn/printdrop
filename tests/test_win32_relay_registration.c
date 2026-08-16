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
    long status = 99L;

    PD_TEST_ASSERT(pd_win32_relay_register_session(NULL, session, secret, &status) ==
                   PD_RELAY_REGISTRATION_INVALID_ARGUMENT);
    PD_TEST_ASSERT(status == 0L);
    PD_TEST_ASSERT(pd_win32_relay_register_session("http://relay.printdrop.app/v1/sessions",
                                                   session,
                                                   secret,
                                                   NULL) ==
                   PD_RELAY_REGISTRATION_INVALID_ARGUMENT);
    PD_TEST_ASSERT(pd_win32_relay_register_session("https://relay.printdrop.app/v1/sessions",
                                                   "short",
                                                   secret,
                                                   NULL) ==
                   PD_RELAY_REGISTRATION_INVALID_SESSION_ID);
    PD_TEST_ASSERT(pd_win32_relay_register_session("https://relay.printdrop.app/v1/sessions",
                                                   session,
                                                   "bad",
                                                   NULL) ==
                   PD_RELAY_REGISTRATION_INVALID_SECRET);

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("Win32 relay registration validation tests passed.");
    return 0;
}

#endif
