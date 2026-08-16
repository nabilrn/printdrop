#ifndef PRINTDROP_SESSION_URL_H
#define PRINTDROP_SESSION_URL_H

#include "printdrop/receiver_session.h"

#include <stddef.h>

typedef enum pd_session_url_result {
    PD_SESSION_URL_OK = 0,
    PD_SESSION_URL_INVALID_ARGUMENT,
    PD_SESSION_URL_SESSION_UNAVAILABLE,
    PD_SESSION_URL_BUFFER_TOO_SMALL
} pd_session_url_result;

pd_session_url_result pd_receiver_session_build_url(const pd_receiver_session *session,
                                                    const char *session_prefix,
                                                    char *output,
                                                    size_t output_capacity,
                                                    size_t *required_capacity);

#endif
