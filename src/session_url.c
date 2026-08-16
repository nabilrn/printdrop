#include "printdrop/session_url.h"

#include <stdint.h>
#include <string.h>

pd_session_url_result pd_receiver_session_build_url(const pd_receiver_session *session,
                                                    const char *session_prefix,
                                                    char *output,
                                                    size_t output_capacity,
                                                    size_t *required_capacity)
{
    size_t prefix_length;
    size_t token_length;
    size_t required;

    if (required_capacity != NULL) {
        *required_capacity = 0U;
    }

    if (session == NULL || session_prefix == NULL || output == NULL) {
        return PD_SESSION_URL_INVALID_ARGUMENT;
    }

    if ((session->state != PD_RECEIVER_SESSION_WAITING &&
         session->state != PD_RECEIVER_SESSION_TRANSFERRING) ||
        session->token[0] == '\0') {
        return PD_SESSION_URL_SESSION_UNAVAILABLE;
    }

    prefix_length = strlen(session_prefix);
    token_length = strlen(session->token);

    if (prefix_length == 0U || token_length != (size_t)PD_SESSION_TOKEN_HEX_CHARS ||
        prefix_length > SIZE_MAX - token_length - 1U) {
        return PD_SESSION_URL_INVALID_ARGUMENT;
    }

    required = prefix_length + token_length + 1U;
    if (required_capacity != NULL) {
        *required_capacity = required;
    }

    if (output_capacity < required) {
        if (output_capacity > 0U) {
            output[0] = '\0';
        }
        return PD_SESSION_URL_BUFFER_TOO_SMALL;
    }

    memcpy(output, session_prefix, prefix_length);
    memcpy(output + prefix_length, session->token, token_length);
    output[prefix_length + token_length] = '\0';
    return PD_SESSION_URL_OK;
}
