#include "printdrop/relay_registration.h"

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

static void test_endpoint_builder(void)
{
    char endpoint[PD_RELAY_REGISTRATION_ENDPOINT_CAPACITY];

    PD_TEST_ASSERT(pd_relay_registration_build_endpoint(
                       "https://relay.printdrop.app",
                       endpoint,
                       sizeof(endpoint)) == PD_RELAY_REGISTRATION_BUILD_OK);
    PD_TEST_ASSERT(strcmp(endpoint,
                          "https://relay.printdrop.app/v1/sessions") == 0);

    PD_TEST_ASSERT(pd_relay_registration_build_endpoint(
                       "https://relay.printdrop.app/",
                       endpoint,
                       sizeof(endpoint)) == PD_RELAY_REGISTRATION_BUILD_OK);
    PD_TEST_ASSERT(strcmp(endpoint,
                          "https://relay.printdrop.app/v1/sessions") == 0);

    PD_TEST_ASSERT(pd_relay_registration_build_endpoint(
                       "http://relay.printdrop.app",
                       endpoint,
                       sizeof(endpoint)) == PD_RELAY_REGISTRATION_BUILD_INVALID_BASE_URL);
    PD_TEST_ASSERT(pd_relay_registration_build_endpoint(
                       "https://relay.printdrop.app?secret=nope",
                       endpoint,
                       sizeof(endpoint)) == PD_RELAY_REGISTRATION_BUILD_INVALID_BASE_URL);
}

static void test_body_builder(void)
{
    static const char session_id[] = "00112233445566778899aabbccddeeff";
    char body[PD_RELAY_REGISTRATION_BODY_CAPACITY];
    char too_small[8] = "stale";
    size_t body_size = 0U;

    PD_TEST_ASSERT(pd_relay_registration_build_body(session_id,
                                                    body,
                                                    sizeof(body),
                                                    &body_size) ==
                   PD_RELAY_REGISTRATION_BUILD_OK);
    PD_TEST_ASSERT(strcmp(body,
                          "{\"session_id\":\"00112233445566778899aabbccddeeff\"}") == 0);
    PD_TEST_ASSERT(body_size == strlen(body));

    PD_TEST_ASSERT(pd_relay_registration_build_body(
                       "00112233445566778899AABBCCDDEEFF",
                       body,
                       sizeof(body),
                       NULL) == PD_RELAY_REGISTRATION_BUILD_INVALID_SESSION_ID);
    PD_TEST_ASSERT(pd_relay_registration_build_body(session_id,
                                                    too_small,
                                                    sizeof(too_small),
                                                    &body_size) ==
                   PD_RELAY_REGISTRATION_BUILD_BUFFER_TOO_SMALL);
    PD_TEST_ASSERT(too_small[0] == '\0');
    PD_TEST_ASSERT(body_size == 0U);
}

int main(void)
{
    test_endpoint_builder();
    test_body_builder();

    if (failures != 0) {
        fprintf(stderr, "%d relay registration assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop relay registration tests passed.");
    return 0;
}
