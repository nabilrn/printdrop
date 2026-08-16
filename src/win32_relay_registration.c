#include "printdrop/win32_relay_registration.h"

#ifdef _WIN32

#include "printdrop/receiver_session.h"

#include <curl/curl.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static bool pd_lower_hex_is_valid(const char *value, size_t expected_length)
{
    size_t index;

    if (value == NULL || strlen(value) != expected_length) {
        return false;
    }

    for (index = 0U; index < expected_length; ++index) {
        char character = value[index];
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

static size_t pd_discard_response(char *buffer, size_t size, size_t count, void *context)
{
    (void)buffer;
    (void)context;
    if (size != 0U && count > SIZE_MAX / size) {
        return 0U;
    }
    return size * count;
}

pd_relay_registration_result pd_win32_relay_register_session(const char *registration_url,
                                                              const char *session_id,
                                                              const char *receiver_secret,
                                                              long *http_status)
{
    static const char auth_prefix[] = "Authorization: Bearer ";
    static const char json_prefix[] = "{\"session_id\":\"";
    static const char json_suffix[] = "\"}";
    char auth_header[sizeof(auth_prefix) + PD_RECEIVER_SECRET_HEX_CHARS];
    char body[sizeof(json_prefix) + PD_SESSION_TOKEN_HEX_CHARS + sizeof(json_suffix) - 1U];
    struct curl_slist *headers = NULL;
    CURL *easy;
    CURLcode code;
    long status = 0L;
    size_t body_length;

    if (http_status != NULL) {
        *http_status = 0L;
    }
    if (registration_url == NULL || session_id == NULL || receiver_secret == NULL) {
        return PD_RELAY_REGISTRATION_INVALID_ARGUMENT;
    }
    if (!pd_lower_hex_is_valid(session_id, (size_t)PD_SESSION_TOKEN_HEX_CHARS)) {
        return PD_RELAY_REGISTRATION_INVALID_SESSION_ID;
    }
    if (!pd_lower_hex_is_valid(receiver_secret, (size_t)PD_RECEIVER_SECRET_HEX_CHARS)) {
        return PD_RELAY_REGISTRATION_INVALID_SECRET;
    }

    memcpy(auth_header, auth_prefix, sizeof(auth_prefix) - 1U);
    memcpy(&auth_header[sizeof(auth_prefix) - 1U],
           receiver_secret,
           (size_t)PD_RECEIVER_SECRET_HEX_CHARS);
    auth_header[(sizeof(auth_prefix) - 1U) + PD_RECEIVER_SECRET_HEX_CHARS] = '\0';

    memcpy(body, json_prefix, sizeof(json_prefix) - 1U);
    memcpy(&body[sizeof(json_prefix) - 1U],
           session_id,
           (size_t)PD_SESSION_TOKEN_HEX_CHARS);
    memcpy(&body[(sizeof(json_prefix) - 1U) + PD_SESSION_TOKEN_HEX_CHARS],
           json_suffix,
           sizeof(json_suffix));
    body_length = strlen(body);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Expect:");
    if (headers == NULL) {
        return PD_RELAY_REGISTRATION_CURL_ERROR;
    }

    easy = curl_easy_init();
    if (easy == NULL) {
        curl_slist_free_all(headers);
        return PD_RELAY_REGISTRATION_CURL_ERROR;
    }

    code = curl_easy_setopt(easy, CURLOPT_URL, registration_url);
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_POST, 1L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_POSTFIELDS, body);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)body_length);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, pd_discard_response);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 15000L);
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
    if (code == CURLE_OK) {
        code = curl_easy_perform(easy);
    }
    if (code == CURLE_OK) {
        code = curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
    }

    curl_easy_cleanup(easy);
    curl_slist_free_all(headers);

    if (http_status != NULL) {
        *http_status = status;
    }
    if (code != CURLE_OK) {
        return PD_RELAY_REGISTRATION_CURL_ERROR;
    }
    if (status != 200L && status != 201L) {
        return PD_RELAY_REGISTRATION_HTTP_ERROR;
    }
    return PD_RELAY_REGISTRATION_OK;
}

#endif
