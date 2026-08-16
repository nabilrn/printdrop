#ifndef PRINTDROP_WIN32_RELAY_REGISTRATION_H
#define PRINTDROP_WIN32_RELAY_REGISTRATION_H

#ifdef _WIN32

#include <stdint.h>

typedef enum pd_relay_registration_result {
    PD_RELAY_REGISTRATION_OK = 0,
    PD_RELAY_REGISTRATION_INVALID_ARGUMENT,
    PD_RELAY_REGISTRATION_INVALID_SESSION_ID,
    PD_RELAY_REGISTRATION_INVALID_SECRET,
    PD_RELAY_REGISTRATION_CURL_ERROR,
    PD_RELAY_REGISTRATION_HTTP_ERROR
} pd_relay_registration_result;

pd_relay_registration_result pd_win32_relay_register_session(const char *registration_url,
                                                              const char *session_id,
                                                              const char *receiver_secret,
                                                              long *http_status);

#endif

#endif
