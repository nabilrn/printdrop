#include "printdrop/relay_registration.h"

#include <stdbool.h>
#include <string.h>

static bool pd_is_lower_hex(const char *value, size_t expected_length)
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

pd_relay_registration_build_result pd_relay_registration_build_endpoint(
    const char *https_base_url,
    char *output,
    size_t output_capacity)
{
    static const char https_scheme[] = "https://";
    static const char http_scheme[] = "http://";
    static const char path[] = "/v1/sessions";
    size_t base_length;
    size_t scheme_length;
    size_t trimmed_length;
    size_t required;

    if (https_base_url == NULL || output == NULL) {
        return PD_RELAY_REGISTRATION_BUILD_INVALID_ARGUMENT;
    }

    base_length = strlen(https_base_url);
    if (base_length > sizeof(https_scheme) - 1U &&
        memcmp(https_base_url, https_scheme, sizeof(https_scheme) - 1U) == 0) {
        scheme_length = sizeof(https_scheme) - 1U;
    } else if (base_length > sizeof(http_scheme) - 1U &&
               memcmp(https_base_url, http_scheme, sizeof(http_scheme) - 1U) == 0) {
        scheme_length = sizeof(http_scheme) - 1U;
    } else {
        return PD_RELAY_REGISTRATION_BUILD_INVALID_BASE_URL;
    }

    if (base_length > (size_t)PD_RELAY_BASE_URL_MAX_BYTES || strchr(https_base_url, '?') != NULL ||
        strchr(https_base_url, '#') != NULL) {
        return PD_RELAY_REGISTRATION_BUILD_INVALID_BASE_URL;
    }

    trimmed_length = base_length;
    while (trimmed_length > scheme_length && https_base_url[trimmed_length - 1U] == '/') {
        trimmed_length -= 1U;
    }
    if (trimmed_length <= scheme_length) {
        return PD_RELAY_REGISTRATION_BUILD_INVALID_BASE_URL;
    }

    required = trimmed_length + (sizeof(path) - 1U) + 1U;
    if (output_capacity < required) {
        if (output_capacity > 0U) {
            output[0] = '\0';
        }
        return PD_RELAY_REGISTRATION_BUILD_BUFFER_TOO_SMALL;
    }

    memcpy(output, https_base_url, trimmed_length);
    memcpy(&output[trimmed_length], path, sizeof(path));
    return PD_RELAY_REGISTRATION_BUILD_OK;
}

pd_relay_registration_build_result pd_relay_registration_build_body(
    const char *session_id,
    char *output,
    size_t output_capacity,
    size_t *body_size)
{
    static const char prefix[] = "{\"session_id\":\"";
    static const char suffix[] = "\"}";
    size_t required;

    if (body_size != NULL) {
        *body_size = 0U;
    }
    if (session_id == NULL || output == NULL) {
        return PD_RELAY_REGISTRATION_BUILD_INVALID_ARGUMENT;
    }
    if (!pd_is_lower_hex(session_id, (size_t)PD_SESSION_TOKEN_HEX_CHARS)) {
        return PD_RELAY_REGISTRATION_BUILD_INVALID_SESSION_ID;
    }

    required = (sizeof(prefix) - 1U) + (size_t)PD_SESSION_TOKEN_HEX_CHARS +
               (sizeof(suffix) - 1U) + 1U;
    if (output_capacity < required) {
        if (output_capacity > 0U) {
            output[0] = '\0';
        }
        return PD_RELAY_REGISTRATION_BUILD_BUFFER_TOO_SMALL;
    }

    memcpy(output, prefix, sizeof(prefix) - 1U);
    memcpy(&output[sizeof(prefix) - 1U], session_id, (size_t)PD_SESSION_TOKEN_HEX_CHARS);
    memcpy(&output[(sizeof(prefix) - 1U) + PD_SESSION_TOKEN_HEX_CHARS],
           suffix,
           sizeof(suffix));

    if (body_size != NULL) {
        *body_size = required - 1U;
    }
    return PD_RELAY_REGISTRATION_BUILD_OK;
}
