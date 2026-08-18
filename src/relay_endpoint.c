#include "printdrop/relay_endpoint.h"

#include "printdrop/relay_registration.h"

#include <stdbool.h>
#include <string.h>

static bool pd_session_id_is_valid(const char *session_id)
{
    size_t index;

    if (session_id == NULL || strlen(session_id) != (size_t)PD_SESSION_TOKEN_HEX_CHARS) {
        return false;
    }
    for (index = 0U; index < (size_t)PD_SESSION_TOKEN_HEX_CHARS; ++index) {
        char value = session_id[index];
        if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) {
            return false;
        }
    }
    return true;
}

static pd_relay_endpoint_result pd_normalize_base(const char *base,
                                                  size_t *base_length,
                                                  size_t *scheme_length,
                                                  bool *secure)
{
    static const char https_prefix[] = "https://";
    static const char http_prefix[] = "http://";
    size_t length;
    size_t prefix_length;
    bool is_secure;

    if (base == NULL || base_length == NULL || scheme_length == NULL || secure == NULL) {
        return PD_RELAY_ENDPOINT_INVALID_ARGUMENT;
    }

    length = strlen(base);
    if (length > sizeof(https_prefix) - 1U &&
        memcmp(base, https_prefix, sizeof(https_prefix) - 1U) == 0) {
        prefix_length = sizeof(https_prefix) - 1U;
        is_secure = true;
    } else if (length > sizeof(http_prefix) - 1U &&
               memcmp(base, http_prefix, sizeof(http_prefix) - 1U) == 0) {
        prefix_length = sizeof(http_prefix) - 1U;
        is_secure = false;
    } else {
        return PD_RELAY_ENDPOINT_INVALID_BASE_URL;
    }

    if (length > (size_t)PD_RELAY_BASE_URL_MAX_BYTES || strchr(base, '?') != NULL ||
        strchr(base, '#') != NULL) {
        return PD_RELAY_ENDPOINT_INVALID_BASE_URL;
    }
    while (length > prefix_length && base[length - 1U] == '/') {
        --length;
    }
    if (length <= prefix_length) {
        return PD_RELAY_ENDPOINT_INVALID_BASE_URL;
    }

    *base_length = length;
    *scheme_length = prefix_length;
    *secure = is_secure;
    return PD_RELAY_ENDPOINT_OK;
}

static pd_relay_endpoint_result pd_build_ws_path(const char *base,
                                                 const char *path_prefix,
                                                 const char *session_id,
                                                 char *output,
                                                 size_t output_capacity)
{
    static const char wss_prefix[] = "wss://";
    static const char ws_prefix[] = "ws://";
    size_t base_length;
    size_t scheme_length;
    size_t host_length;
    size_t path_length;
    size_t output_scheme_length;
    size_t required;
    const char *output_scheme;
    bool secure;
    pd_relay_endpoint_result result;

    if (path_prefix == NULL || output == NULL) {
        return PD_RELAY_ENDPOINT_INVALID_ARGUMENT;
    }
    if (!pd_session_id_is_valid(session_id)) {
        return PD_RELAY_ENDPOINT_INVALID_SESSION_ID;
    }
    result = pd_normalize_base(base, &base_length, &scheme_length, &secure);
    if (result != PD_RELAY_ENDPOINT_OK) {
        return result;
    }

    output_scheme = secure ? wss_prefix : ws_prefix;
    output_scheme_length = secure ? sizeof(wss_prefix) - 1U : sizeof(ws_prefix) - 1U;
    host_length = base_length - scheme_length;
    path_length = strlen(path_prefix);
    required = output_scheme_length + host_length + path_length +
               (size_t)PD_SESSION_TOKEN_HEX_CHARS + 1U;
    if (required > output_capacity || required > (size_t)PD_RELAY_ENDPOINT_CAPACITY) {
        if (output_capacity != 0U) {
            output[0] = '\0';
        }
        return PD_RELAY_ENDPOINT_BUFFER_TOO_SMALL;
    }

    memcpy(output, output_scheme, output_scheme_length);
    memcpy(&output[output_scheme_length], &base[scheme_length], host_length);
    memcpy(&output[output_scheme_length + host_length], path_prefix, path_length);
    memcpy(&output[output_scheme_length + host_length + path_length],
           session_id,
           (size_t)PD_SESSION_TOKEN_HEX_CHARS);
    output[required - 1U] = '\0';
    return PD_RELAY_ENDPOINT_OK;
}

pd_relay_endpoint_result pd_relay_build_receiver_wss_url(const char *https_base_url,
                                                         const char *session_id,
                                                         char *output,
                                                         size_t output_capacity)
{
    return pd_build_ws_path(https_base_url,
                            "/v1/receiver/",
                            session_id,
                            output,
                            output_capacity);
}

pd_relay_endpoint_result pd_relay_build_sender_wss_url(const char *https_base_url,
                                                       const char *session_id,
                                                       char *output,
                                                       size_t output_capacity)
{
    return pd_build_ws_path(https_base_url,
                            "/v1/sender/",
                            session_id,
                            output,
                            output_capacity);
}
