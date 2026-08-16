#ifndef PRINTDROP_WIN32_RELAY_CLIENT_H
#define PRINTDROP_WIN32_RELAY_CLIENT_H

#ifdef _WIN32

#include <stdbool.h>
#include <stddef.h>

#define PD_RELAY_URL_MAX_BYTES 511U
#define PD_RELAY_URL_CAPACITY (PD_RELAY_URL_MAX_BYTES + 1U)

typedef enum pd_relay_client_state {
    PD_RELAY_CLIENT_EMPTY = 0,
    PD_RELAY_CLIENT_PREPARED,
    PD_RELAY_CLIENT_CONNECTED,
    PD_RELAY_CLIENT_CLOSED,
    PD_RELAY_CLIENT_FAILED
} pd_relay_client_state;

typedef enum pd_relay_client_result {
    PD_RELAY_CLIENT_OK = 0,
    PD_RELAY_CLIENT_INVALID_ARGUMENT,
    PD_RELAY_CLIENT_INVALID_URL,
    PD_RELAY_CLIENT_CURL_ERROR
} pd_relay_client_result;

typedef struct pd_win32_relay_client {
    void *easy_handle;
    char url[PD_RELAY_URL_CAPACITY];
    pd_relay_client_state state;
} pd_win32_relay_client;

bool pd_win32_curl_global_init(void);
void pd_win32_curl_global_cleanup(void);
pd_relay_client_result pd_win32_relay_client_init(pd_win32_relay_client *client,
                                                   const char *wss_url);
void pd_win32_relay_client_cleanup(pd_win32_relay_client *client);
const char *pd_win32_relay_client_curl_version(void);
const char *pd_win32_relay_client_ssl_backend(void);

#endif

#endif
