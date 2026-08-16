#ifndef PRINTDROP_WIN32_RELAY_CLIENT_H
#define PRINTDROP_WIN32_RELAY_CLIENT_H

#ifdef _WIN32

#include "printdrop/receiver_session.h"
#include "printdrop/relay_message.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PD_RELAY_URL_MAX_BYTES 511U
#define PD_RELAY_URL_CAPACITY (PD_RELAY_URL_MAX_BYTES + 1U)
#define PD_RELAY_ERROR_CAPACITY 256U
#define PD_RELAY_IO_TIMEOUT_MS 30000L

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
    PD_RELAY_CLIENT_INVALID_SECRET,
    PD_RELAY_CLIENT_INVALID_STATE,
    PD_RELAY_CLIENT_BUFFER_TOO_SMALL,
    PD_RELAY_CLIENT_PROTOCOL_ERROR,
    PD_RELAY_CLIENT_TIMEOUT,
    PD_RELAY_CLIENT_CLOSED_RESULT,
    PD_RELAY_CLIENT_CURL_ERROR
} pd_relay_client_result;

typedef struct pd_win32_relay_client {
    void *easy_handle;
    void *request_headers;
    uint8_t *message_buffer;
    char url[PD_RELAY_URL_CAPACITY];
    char error_text[PD_RELAY_ERROR_CAPACITY];
    pd_relay_client_state state;
} pd_win32_relay_client;

bool pd_win32_curl_global_init(void);
void pd_win32_curl_global_cleanup(void);
pd_relay_client_result pd_win32_relay_client_init(pd_win32_relay_client *client,
                                                   const char *wss_url,
                                                   const char *receiver_secret);
pd_relay_client_result pd_win32_relay_client_connect(pd_win32_relay_client *client);
pd_relay_client_result pd_win32_relay_client_send_frame(pd_win32_relay_client *client,
                                                         const pd_frame_header *header,
                                                         const uint8_t *payload,
                                                         size_t payload_size);
pd_relay_client_result pd_win32_relay_client_receive_frame(pd_win32_relay_client *client,
                                                            pd_relay_message_view *view);
void pd_win32_relay_client_cleanup(pd_win32_relay_client *client);
const char *pd_win32_relay_client_last_error(const pd_win32_relay_client *client);
const char *pd_win32_relay_client_curl_version(void);
const char *pd_win32_relay_client_ssl_backend(void);

#endif

#endif
