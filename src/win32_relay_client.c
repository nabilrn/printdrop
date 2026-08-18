#include "printdrop/win32_relay_client.h"

#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>
#include <curl/curl.h>

#include <stdio.h>
#include <string.h>

static bool pd_insecure_http_enabled(void)
{
    char value[2];
    DWORD length = GetEnvironmentVariableA("PRINTDROP_ALLOW_INSECURE_HTTP",
                                           value,
                                           (DWORD)sizeof(value));
    return length == 1U && value[0] == '1';
}

static bool pd_relay_url_is_allowed(const char *url)
{
    static const char secure_prefix[] = "wss://";
    static const char insecure_prefix[] = "ws://";
    size_t length;

    if (url == NULL) {
        return false;
    }

    length = strlen(url);
    if (length > sizeof(secure_prefix) - 1U &&
        length <= (size_t)PD_RELAY_URL_MAX_BYTES &&
        memcmp(url, secure_prefix, sizeof(secure_prefix) - 1U) == 0) {
        return true;
    }
    return pd_insecure_http_enabled() && length > sizeof(insecure_prefix) - 1U &&
           length <= (size_t)PD_RELAY_URL_MAX_BYTES &&
           memcmp(url, insecure_prefix, sizeof(insecure_prefix) - 1U) == 0;
}

static bool pd_receiver_secret_is_valid(const char *secret)
{
    size_t index;

    if (secret == NULL || strlen(secret) != (size_t)PD_RECEIVER_SECRET_HEX_CHARS) {
        return false;
    }

    for (index = 0U; index < (size_t)PD_RECEIVER_SECRET_HEX_CHARS; ++index) {
        char value = secret[index];
        if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) {
            return false;
        }
    }
    return true;
}

static pd_relay_client_result pd_relay_wait_socket(pd_win32_relay_client *client,
                                                    bool writable)
{
    curl_socket_t socket_handle = CURL_SOCKET_BAD;
    fd_set read_set;
    fd_set write_set;
    struct timeval timeout;
    int selected;

    if (curl_easy_getinfo((CURL *)client->easy_handle,
                          CURLINFO_ACTIVESOCKET,
                          &socket_handle) != CURLE_OK ||
        socket_handle == CURL_SOCKET_BAD) {
        return PD_RELAY_CLIENT_CURL_ERROR;
    }

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    if (writable) {
        FD_SET(socket_handle, &write_set);
    } else {
        FD_SET(socket_handle, &read_set);
    }

    timeout.tv_sec = (long)(PD_RELAY_IO_TIMEOUT_MS / 1000L);
    timeout.tv_usec = (long)((PD_RELAY_IO_TIMEOUT_MS % 1000L) * 1000L);
    selected = select(0,
                      writable ? NULL : &read_set,
                      writable ? &write_set : NULL,
                      NULL,
                      &timeout);
    if (selected == 0) {
        return PD_RELAY_CLIENT_TIMEOUT;
    }
    if (selected == SOCKET_ERROR) {
        return PD_RELAY_CLIENT_CURL_ERROR;
    }
    return PD_RELAY_CLIENT_OK;
}

static pd_relay_client_result pd_relay_curl_failure(pd_win32_relay_client *client, CURLcode code)
{
    if (code == CURLE_GOT_NOTHING) {
        client->state = PD_RELAY_CLIENT_CLOSED;
        return PD_RELAY_CLIENT_CLOSED_RESULT;
    }
    client->state = PD_RELAY_CLIENT_FAILED;
    return PD_RELAY_CLIENT_CURL_ERROR;
}

bool pd_win32_curl_global_init(void)
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

void pd_win32_curl_global_cleanup(void)
{
    curl_global_cleanup();
}

pd_relay_client_result pd_win32_relay_client_init(pd_win32_relay_client *client,
                                                   const char *wss_url,
                                                   const char *receiver_secret)
{
    static const char auth_prefix[] = "Authorization: Bearer ";
    char auth_header[sizeof(auth_prefix) + PD_RECEIVER_SECRET_HEX_CHARS];
    CURL *easy;
    struct curl_slist *headers = NULL;
    CURLcode code;
    size_t url_length;

    if (client == NULL || wss_url == NULL || receiver_secret == NULL) {
        return PD_RELAY_CLIENT_INVALID_ARGUMENT;
    }

    memset(client, 0, sizeof(*client));

    if (!pd_relay_url_is_allowed(wss_url)) {
        return PD_RELAY_CLIENT_INVALID_URL;
    }
    if (!pd_receiver_secret_is_valid(receiver_secret)) {
        return PD_RELAY_CLIENT_INVALID_SECRET;
    }

    client->message_buffer = (uint8_t *)HeapAlloc(GetProcessHeap(),
                                                  0U,
                                                  (SIZE_T)PD_RELAY_WS_MESSAGE_MAX_SIZE);
    if (client->message_buffer == NULL) {
        client->state = PD_RELAY_CLIENT_FAILED;
        return PD_RELAY_CLIENT_CURL_ERROR;
    }

    easy = curl_easy_init();
    if (easy == NULL) {
        HeapFree(GetProcessHeap(), 0U, client->message_buffer);
        client->message_buffer = NULL;
        client->state = PD_RELAY_CLIENT_FAILED;
        return PD_RELAY_CLIENT_CURL_ERROR;
    }

    memcpy(auth_header, auth_prefix, sizeof(auth_prefix) - 1U);
    memcpy(&auth_header[sizeof(auth_prefix) - 1U],
           receiver_secret,
           (size_t)PD_RECEIVER_SECRET_HEX_CHARS);
    auth_header[(sizeof(auth_prefix) - 1U) + PD_RECEIVER_SECRET_HEX_CHARS] = '\0';
    headers = curl_slist_append(NULL, auth_header);
    if (headers == NULL) {
        curl_easy_cleanup(easy);
        HeapFree(GetProcessHeap(), 0U, client->message_buffer);
        client->message_buffer = NULL;
        client->state = PD_RELAY_CLIENT_FAILED;
        return PD_RELAY_CLIENT_CURL_ERROR;
    }

    client->error_text[0] = '\0';
    code = curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, client->error_text);
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_URL, wss_url);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 2L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 0L);
    }

    if (code != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(easy);
        HeapFree(GetProcessHeap(), 0U, client->message_buffer);
        client->message_buffer = NULL;
        client->state = PD_RELAY_CLIENT_FAILED;
        return PD_RELAY_CLIENT_CURL_ERROR;
    }

    url_length = strlen(wss_url);
    memcpy(client->url, wss_url, url_length + 1U);
    client->easy_handle = easy;
    client->request_headers = headers;
    client->state = PD_RELAY_CLIENT_PREPARED;
    return PD_RELAY_CLIENT_OK;
}

pd_relay_client_result pd_win32_relay_client_connect(pd_win32_relay_client *client)
{
    CURLcode code;

    if (client == NULL || client->easy_handle == NULL ||
        client->state != PD_RELAY_CLIENT_PREPARED) {
        return PD_RELAY_CLIENT_INVALID_STATE;
    }

    client->error_text[0] = '\0';
    code = curl_easy_perform((CURL *)client->easy_handle);
    if (code != CURLE_OK) {
        return pd_relay_curl_failure(client, code);
    }

    client->state = PD_RELAY_CLIENT_CONNECTED;
    return PD_RELAY_CLIENT_OK;
}

pd_relay_client_result pd_win32_relay_client_send_frame(pd_win32_relay_client *client,
                                                         const pd_frame_header *header,
                                                         const uint8_t *payload,
                                                         size_t payload_size)
{
    size_t message_size = 0U;
    size_t offset = 0U;

    if (client == NULL || header == NULL ||
        (payload == NULL && payload_size != 0U)) {
        return PD_RELAY_CLIENT_INVALID_ARGUMENT;
    }
    if (client->state != PD_RELAY_CLIENT_CONNECTED || client->easy_handle == NULL ||
        client->message_buffer == NULL) {
        return PD_RELAY_CLIENT_INVALID_STATE;
    }
    if (pd_relay_message_encode(header,
                                payload,
                                payload_size,
                                client->message_buffer,
                                (size_t)PD_RELAY_WS_MESSAGE_MAX_SIZE,
                                &message_size) != PD_RELAY_MESSAGE_OK) {
        return PD_RELAY_CLIENT_PROTOCOL_ERROR;
    }

    while (offset < message_size) {
        size_t sent = 0U;
        CURLcode code = curl_ws_send((CURL *)client->easy_handle,
                                     &client->message_buffer[offset],
                                     message_size - offset,
                                     &sent,
                                     0,
                                     CURLWS_BINARY);
        if (code == CURLE_AGAIN) {
            pd_relay_client_result wait_result = pd_relay_wait_socket(client, true);
            if (wait_result != PD_RELAY_CLIENT_OK) {
                client->state = wait_result == PD_RELAY_CLIENT_TIMEOUT
                                    ? PD_RELAY_CLIENT_CONNECTED
                                    : PD_RELAY_CLIENT_FAILED;
                return wait_result;
            }
            continue;
        }
        if (code != CURLE_OK) {
            return pd_relay_curl_failure(client, code);
        }
        if (sent == 0U) {
            client->state = PD_RELAY_CLIENT_FAILED;
            return PD_RELAY_CLIENT_CURL_ERROR;
        }
        offset += sent;
    }

    return PD_RELAY_CLIENT_OK;
}

pd_relay_client_result pd_win32_relay_client_receive_frame(pd_win32_relay_client *client,
                                                            pd_relay_message_view *view)
{
    size_t offset = 0U;

    if (client == NULL || view == NULL) {
        return PD_RELAY_CLIENT_INVALID_ARGUMENT;
    }
    if (client->state != PD_RELAY_CLIENT_CONNECTED || client->easy_handle == NULL ||
        client->message_buffer == NULL) {
        return PD_RELAY_CLIENT_INVALID_STATE;
    }

    for (;;) {
        size_t received = 0U;
        const struct curl_ws_frame *meta = NULL;
        CURLcode code = curl_ws_recv((CURL *)client->easy_handle,
                                     &client->message_buffer[offset],
                                     (size_t)PD_RELAY_WS_MESSAGE_MAX_SIZE - offset,
                                     &received,
                                     &meta);

        if (code == CURLE_AGAIN) {
            pd_relay_client_result wait_result = pd_relay_wait_socket(client, false);
            if (wait_result != PD_RELAY_CLIENT_OK) {
                client->state = wait_result == PD_RELAY_CLIENT_TIMEOUT
                                    ? PD_RELAY_CLIENT_CONNECTED
                                    : PD_RELAY_CLIENT_FAILED;
                return wait_result;
            }
            continue;
        }
        if (code != CURLE_OK) {
            return pd_relay_curl_failure(client, code);
        }
        if (meta == NULL) {
            client->state = PD_RELAY_CLIENT_FAILED;
            return PD_RELAY_CLIENT_PROTOCOL_ERROR;
        }

        if ((meta->flags & CURLWS_CLOSE) != 0) {
            client->state = PD_RELAY_CLIENT_CLOSED;
            return PD_RELAY_CLIENT_CLOSED_RESULT;
        }
        if ((meta->flags & (CURLWS_PING | CURLWS_PONG)) != 0) {
            if (meta->bytesleft == 0) {
                offset = 0U;
            }
            continue;
        }
        if ((meta->flags & CURLWS_BINARY) == 0 || (meta->flags & CURLWS_CONT) != 0) {
            client->state = PD_RELAY_CLIENT_FAILED;
            return PD_RELAY_CLIENT_PROTOCOL_ERROR;
        }
        if (received > (size_t)PD_RELAY_WS_MESSAGE_MAX_SIZE - offset) {
            client->state = PD_RELAY_CLIENT_FAILED;
            return PD_RELAY_CLIENT_BUFFER_TOO_SMALL;
        }
        offset += received;

        if (meta->bytesleft < 0 ||
            (uint64_t)meta->bytesleft >
                (uint64_t)((size_t)PD_RELAY_WS_MESSAGE_MAX_SIZE - offset)) {
            client->state = PD_RELAY_CLIENT_FAILED;
            return PD_RELAY_CLIENT_BUFFER_TOO_SMALL;
        }
        if (meta->bytesleft != 0) {
            continue;
        }

        if (pd_relay_message_decode(client->message_buffer, offset, view) !=
            PD_RELAY_MESSAGE_OK) {
            client->state = PD_RELAY_CLIENT_FAILED;
            return PD_RELAY_CLIENT_PROTOCOL_ERROR;
        }
        return PD_RELAY_CLIENT_OK;
    }
}

void pd_win32_relay_client_cleanup(pd_win32_relay_client *client)
{
    if (client == NULL) {
        return;
    }

    if (client->state == PD_RELAY_CLIENT_CONNECTED && client->easy_handle != NULL) {
        size_t sent = 0U;
        (void)curl_ws_send((CURL *)client->easy_handle, "", 0U, &sent, 0, CURLWS_CLOSE);
    }
    if (client->request_headers != NULL) {
        curl_slist_free_all((struct curl_slist *)client->request_headers);
        client->request_headers = NULL;
    }
    if (client->easy_handle != NULL) {
        curl_easy_cleanup((CURL *)client->easy_handle);
        client->easy_handle = NULL;
    }
    if (client->message_buffer != NULL) {
        HeapFree(GetProcessHeap(), 0U, client->message_buffer);
        client->message_buffer = NULL;
    }
    memset(client->url, 0, sizeof(client->url));
    client->state = PD_RELAY_CLIENT_CLOSED;
}

const char *pd_win32_relay_client_last_error(const pd_win32_relay_client *client)
{
    if (client == NULL || client->error_text[0] == '\0') {
        return NULL;
    }
    return client->error_text;
}

const char *pd_win32_relay_client_curl_version(void)
{
    const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    return info == NULL ? NULL : info->version;
}

const char *pd_win32_relay_client_ssl_backend(void)
{
    const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    return info == NULL ? NULL : info->ssl_version;
}

#endif
