#ifndef PRINTDROP_RELAY_REGISTRATION_H
#define PRINTDROP_RELAY_REGISTRATION_H

#include "printdrop/receiver_session.h"

#include <stddef.h>

#define PD_RELAY_BASE_URL_MAX_BYTES 447U
#define PD_RELAY_REGISTRATION_ENDPOINT_CAPACITY 512U
#define PD_RELAY_REGISTRATION_BODY_CAPACITY 64U

typedef enum pd_relay_registration_build_result {
    PD_RELAY_REGISTRATION_BUILD_OK = 0,
    PD_RELAY_REGISTRATION_BUILD_INVALID_ARGUMENT,
    PD_RELAY_REGISTRATION_BUILD_INVALID_BASE_URL,
    PD_RELAY_REGISTRATION_BUILD_INVALID_SESSION_ID,
    PD_RELAY_REGISTRATION_BUILD_BUFFER_TOO_SMALL
} pd_relay_registration_build_result;

pd_relay_registration_build_result pd_relay_registration_build_endpoint(
    const char *https_base_url,
    char *output,
    size_t output_capacity);
pd_relay_registration_build_result pd_relay_registration_build_body(
    const char *session_id,
    char *output,
    size_t output_capacity,
    size_t *body_size);

#endif
