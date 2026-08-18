#include "printdrop/relay_endpoint.h"

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
    static const char session[] = "00112233445566778899aabbccddeeff";
    char output[PD_RELAY_ENDPOINT_CAPACITY];
    char small[8] = "stale";

    PD_TEST_ASSERT(pd_relay_build_receiver_wss_url("https://relay.printdrop.app///",
                                                   session,
                                                   output,
                                                   sizeof(output)) == PD_RELAY_ENDPOINT_OK);
    PD_TEST_ASSERT(strcmp(output,
                          "wss://relay.printdrop.app/v1/receiver/00112233445566778899aabbccddeeff") ==
                   0);
    PD_TEST_ASSERT(pd_relay_build_sender_wss_url("https://relay.printdrop.app",
                                                 session,
                                                 output,
                                                 sizeof(output)) == PD_RELAY_ENDPOINT_OK);
    PD_TEST_ASSERT(strcmp(output,
                          "wss://relay.printdrop.app/v1/sender/00112233445566778899aabbccddeeff") ==
                   0);
    PD_TEST_ASSERT(pd_relay_build_receiver_wss_url("http://192.168.1.10:8080",
                                                   session,
                                                   output,
                                                   sizeof(output)) == PD_RELAY_ENDPOINT_OK);
    PD_TEST_ASSERT(strcmp(output,
                          "ws://192.168.1.10:8080/v1/receiver/00112233445566778899aabbccddeeff") ==
                   0);
    PD_TEST_ASSERT(pd_relay_build_receiver_wss_url("ftp://relay.printdrop.app",
                                                   session,
                                                   output,
                                                   sizeof(output)) ==
                   PD_RELAY_ENDPOINT_INVALID_BASE_URL);
    PD_TEST_ASSERT(pd_relay_build_receiver_wss_url("https://relay.printdrop.app?x=1",
                                                   session,
                                                   output,
                                                   sizeof(output)) ==
                   PD_RELAY_ENDPOINT_INVALID_BASE_URL);
    PD_TEST_ASSERT(pd_relay_build_receiver_wss_url("https://relay.printdrop.app",
                                                   "00112233445566778899AABBCCDDEEFF",
                                                   output,
                                                   sizeof(output)) ==
                   PD_RELAY_ENDPOINT_INVALID_SESSION_ID);
    PD_TEST_ASSERT(pd_relay_build_sender_wss_url("https://relay.printdrop.app",
                                                 session,
                                                 small,
                                                 sizeof(small)) ==
                   PD_RELAY_ENDPOINT_BUFFER_TOO_SMALL);
    PD_TEST_ASSERT(small[0] == '\0');

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }
    puts("Relay endpoint tests passed.");
    return 0;
}
