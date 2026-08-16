#include "printdrop/win32_relay_client.h"

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
    pd_win32_relay_client client;
    const char *curl_version;
    const char *ssl_backend;

    PD_TEST_ASSERT(pd_win32_curl_global_init());

    curl_version = pd_win32_relay_client_curl_version();
    ssl_backend = pd_win32_relay_client_ssl_backend();
    PD_TEST_ASSERT(curl_version != NULL);
    PD_TEST_ASSERT(ssl_backend != NULL);
    if (curl_version != NULL) {
        PD_TEST_ASSERT(strstr(curl_version, "8.21.0") != NULL);
    }
    if (ssl_backend != NULL) {
        PD_TEST_ASSERT(strstr(ssl_backend, "Schannel") != NULL);
    }

    PD_TEST_ASSERT(pd_win32_relay_client_init(
                       &client,
                       "wss://relay.printdrop.app/v1/receiver/0123456789abcdef") ==
                   PD_RELAY_CLIENT_OK);
    PD_TEST_ASSERT(client.state == PD_RELAY_CLIENT_PREPARED);
    PD_TEST_ASSERT(client.easy_handle != NULL);
    PD_TEST_ASSERT(strcmp(client.url,
                          "wss://relay.printdrop.app/v1/receiver/0123456789abcdef") == 0);
    pd_win32_relay_client_cleanup(&client);
    PD_TEST_ASSERT(client.state == PD_RELAY_CLIENT_CLOSED);
    PD_TEST_ASSERT(client.easy_handle == NULL);
    PD_TEST_ASSERT(client.url[0] == '\0');

    PD_TEST_ASSERT(pd_win32_relay_client_init(&client, "https://relay.printdrop.app") ==
                   PD_RELAY_CLIENT_INVALID_URL);
    PD_TEST_ASSERT(pd_win32_relay_client_init(&client, "wss://") ==
                   PD_RELAY_CLIENT_INVALID_URL);
    PD_TEST_ASSERT(pd_win32_relay_client_init(NULL, "wss://relay.printdrop.app") ==
                   PD_RELAY_CLIENT_INVALID_ARGUMENT);

    pd_win32_curl_global_cleanup();

    if (failures != 0) {
        fprintf(stderr, "%d Win32 relay client assertion(s) failed.\n", failures);
        return 1;
    }

    puts("All PrintDrop Win32 relay client tests passed.");
    return 0;
}
