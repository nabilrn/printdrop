#include "printdrop/win32_relay_registration.h"

#ifdef _WIN32

#include "printdrop/relay_registration.h"
#include "printdrop/secure_zero.h"

#include <windows.h>
#include <curl/curl.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define PD_AUTH_PREFIX "Authorization: Bearer "
#define PD_AUTH_HEADER_CAPACITY \
    ((sizeof(PD_AUTH_PREFIX) - 1U) + PD_RECEIVER_SECRET_HEX_CHARS + 1U)

static bool pd_secret_is_lower_hex(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != (size_t)PD_RECEIVER_SECRET_HEX_CHARS) {
        return false;
    }
    for (index = 0U; index < (size_t)PD_RECEIVER_SECRET_HEX_CHARS; ++index) {
        char character = value[index];
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool pd_insecure_http_enabled(void)
{
    char value[2];
    DWORD length = GetEnvironmentVariableA("PRINTDROP_ALLOW_INSECURE_HTTP",
                                           value,
                                           (DWORD)sizeof(value));
    return length == 1U && value[0] == '1';
}

static bool pd_base_url_requires_insecure_opt_in(const char *base_url)
{
    static const char prefix[] = "http://";
    size_t length;

    if (base_url == NULL) {
        return false;
    }
    length = strlen(base_url);
    return length > sizeof(prefix) - 1U &&
           memcmp(base_url, prefix, sizeof(prefix) - 1U) == 0;
}

static size_t pd_discard_response(char *data, size_t size, size_t count, void *context)
{
    (void)data;
    (void)context;

    if (size != 0U && count > SIZE_MAX / size) {
        return 0U;
    }
    return size * count;
}

static void pd_copy_error(char *destination,
                          size_t destination_capacity,
                          const char *source)
{
    size_t source_length;
    size_t copy_length;

    if (destination == NULL || destination_capacity == 0U) {
        return;
    }
    destination[0] = '\0';
    if (source == NULL) {
        return;
    }

    source_length = strlen(source);
    copy_length = source_length < destination_capacity - 1U
                      ? source_length
                      : destination_capacity - 1U;
    if (copy_length != 0U) {
        memcpy(destination, source, copy_length);
    }
    destination[copy_length] = '\0';
}

pd_win32_relay_registration_result pd_win32_relay_register_session(
    const char *https_base_url,
    const char *session_id,
    const char *receiver_secret,
    char *error_text,
    size_t error_capacity)
{
    static const char content_type[] = "Content-Type: application/json";
    char endpoint[PD_RELAY_REGISTRATION_ENDPOINT_CAPACITY];
    char body[PD_RELAY_REGISTRATION_BODY_CAPACITY];
    char auth_header[PD_AUTH_HEADER_CAPACITY];
    char curl_error[CURL_ERROR_SIZE];
    size_t body_size = 0U;
    CURL *easy = NULL;
    struct curl_slist *headers = NULL;
    CURLcode code = CURLE_OK;
    long response_code = 0L;
    pd_win32_relay_registration_result result = PD_WIN32_RELAY_REGISTRATION_CURL_ERROR;

    if (error_text != NULL && error_capacity > 0U) {
        error_text[0] = '\0';
    }
    if (https_base_url == NULL || session_id == NULL || receiver_secret == NULL) {
        return PD_WIN32_RELAY_REGISTRATION_INVALID_ARGUMENT;
    }
    if ((pd_base_url_requires_insecure_opt_in(https_base_url) && !pd_insecure_http_enabled()) ||
        !pd_secret_is_lower_hex(receiver_secret) ||
        pd_relay_registration_build_endpoint(https_base_url,
                                             endpoint,
                                             sizeof(endpoint)) !=
            PD_RELAY_REGISTRATION_BUILD_OK ||
        pd_relay_registration_build_body(session_id,
                                         body,
                                         sizeof(body),
                                         &body_size) !=
            PD_RELAY_REGISTRATION_BUILD_OK) {
        return PD_WIN32_RELAY_REGISTRATION_INVALID_REQUEST;
    }

    memcpy(auth_header, PD_AUTH_PREFIX, sizeof(PD_AUTH_PREFIX) - 1U);
    memcpy(&auth_header[sizeof(PD_AUTH_PREFIX) - 1U],
           receiver_secret,
           (size_t)PD_RECEIVER_SECRET_HEX_CHARS);
    auth_header[(sizeof(PD_AUTH_PREFIX) - 1U) + PD_RECEIVER_SECRET_HEX_CHARS] = '\0';
    memset(curl_error, 0, sizeof(curl_error));

    easy = curl_easy_init();
    if (easy == NULL) {
        pd_secure_zero(auth_header, sizeof(auth_header));
        return PD_WIN32_RELAY_REGISTRATION_CURL_ERROR;
    }

    headers = curl_slist_append(NULL, content_type);
    if (headers != NULL) {
        struct curl_slist *updated = curl_slist_append(headers, auth_header);
        if (updated == NULL) {
            curl_slist_free_all(headers);
            headers = NULL;
        } else {
            headers = updated;
        }
    }
    if (headers == NULL) {
        curl_easy_cleanup(easy);
        pd_secure_zero(auth_header, sizeof(auth_header));
        return PD_WIN32_RELAY_REGISTRATION_CURL_ERROR;
    }

    code = curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, curl_error);
    if (code == CURLE_OK) {
        code = curl_easy_setopt(easy, CURLOPT_URL, endpoint);
    }
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
        code = curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)body_size);
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
        code = curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    }

    if (code == CURLE_OK) {
        code = curl_easy_perform(easy);
    }
    if (code != CURLE_OK) {
        const char *message = curl_error[0] != '\0' ? curl_error : curl_easy_strerror(code);
        pd_copy_error(error_text, error_capacity, message);
        result = PD_WIN32_RELAY_REGISTRATION_CURL_ERROR;
        goto cleanup;
    }

    code = curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &response_code);
    if (code != CURLE_OK) {
        pd_copy_error(error_text, error_capacity, curl_easy_strerror(code));
        result = PD_WIN32_RELAY_REGISTRATION_CURL_ERROR;
        goto cleanup;
    }

    switch (response_code) {
    case 200L:
    case 201L:
        result = PD_WIN32_RELAY_REGISTRATION_OK;
        break;
    case 401L:
    case 403L:
        result = PD_WIN32_RELAY_REGISTRATION_AUTH_REJECTED;
        break;
    case 409L:
        result = PD_WIN32_RELAY_REGISTRATION_CONFLICT;
        break;
    default:
        result = PD_WIN32_RELAY_REGISTRATION_HTTP_ERROR;
        break;
    }

cleanup:
    curl_slist_free_all(headers);
    curl_easy_cleanup(easy);
    pd_secure_zero(auth_header, sizeof(auth_header));
    pd_secure_zero(curl_error, sizeof(curl_error));
    return result;
}

#endif
