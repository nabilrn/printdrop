#ifndef PRINTDROP_RELAY_ENDPOINT_H
#define PRINTDROP_RELAY_ENDPOINT_H

#include "printdrop/receiver_session.h"

#include <stddef.h>

#define PD_RELAY_BASE_URL_MAX_BYTES 255U
#define PD_RELAY_ENDPOINT_MAX_BYTES 511U
#define PD_RELAY_ENDPOINT_CAPACITY (PD_RELAY_ENDPOINT_MAX_BYTES + 1U)

typedef enum pd_relay_endpoint_result {
    PD_RELAY_ENDPOINT_OK = 0,
    PD_RELAY_ENDPOINT_INVALID_ARGUMENT,
    PD_RELAY_ENDPOINT_INVALID_BASE_URL,
    PD_RELAY_ENDPOINT_INVALID_SESSION_ID,
    PD_RELAY_ENDPOINT_BUFFER_TOO_SMALL
} pd_relay_endpoint_result;

pd_relay_endpoint_result pd_relay_build_registration_url(const char *https_base_url,
                                                         char *output,
                                                         size_t output_capacity);
pd_relay_endpoint_result pd_relay_build_receiver_wss_url(const char *https_base_url,
                                                         const char *session_id,
                                                         char *output,
                                                         size_t output_capacity);
pd_relay_endpoint_result pd_relay_build_sender_wss_url(const char *https_base_url,
                                                       const char *session_id,
                                                       char *output,
                                                       size_t output_capacity);

#endif
