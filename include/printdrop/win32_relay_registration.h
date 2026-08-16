#ifndef PRINTDROP_WIN32_RELAY_REGISTRATION_H
#define PRINTDROP_WIN32_RELAY_REGISTRATION_H

#ifdef _WIN32

#include "printdrop/receiver_session.h"

#include <stddef.h>

typedef enum pd_win32_relay_registration_result {
    PD_WIN32_RELAY_REGISTRATION_OK = 0,
    PD_WIN32_RELAY_REGISTRATION_INVALID_ARGUMENT,
    PD_WIN32_RELAY_REGISTRATION_INVALID_REQUEST,
    PD_WIN32_RELAY_REGISTRATION_AUTH_REJECTED,
    PD_WIN32_RELAY_REGISTRATION_CONFLICT,
    PD_WIN32_RELAY_REGISTRATION_HTTP_ERROR,
    PD_WIN32_RELAY_REGISTRATION_CURL_ERROR
} pd_win32_relay_registration_result;

pd_win32_relay_registration_result pd_win32_relay_register_session(
    const char *https_base_url,
    const char *session_id,
    const char *receiver_secret,
    char *error_text,
    size_t error_capacity);

#endif

#endif
