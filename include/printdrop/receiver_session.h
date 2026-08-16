#ifndef PRINTDROP_RECEIVER_SESSION_H
#define PRINTDROP_RECEIVER_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PD_SESSION_TOKEN_BYTES 16U
#define PD_SESSION_TOKEN_HEX_CHARS (PD_SESSION_TOKEN_BYTES * 2U)
#define PD_SESSION_TOKEN_CAPACITY (PD_SESSION_TOKEN_HEX_CHARS + 1U)

typedef enum pd_receiver_session_state {
    PD_RECEIVER_SESSION_INACTIVE = 0,
    PD_RECEIVER_SESSION_WAITING,
    PD_RECEIVER_SESSION_TRANSFERRING,
    PD_RECEIVER_SESSION_EXPIRED,
    PD_RECEIVER_SESSION_CLOSED
} pd_receiver_session_state;

typedef enum pd_receiver_session_result {
    PD_RECEIVER_SESSION_OK = 0,
    PD_RECEIVER_SESSION_INVALID_ARGUMENT,
    PD_RECEIVER_SESSION_INVALID_STATE,
    PD_RECEIVER_SESSION_ENTROPY_FAILURE,
    PD_RECEIVER_SESSION_CLOCK_OVERFLOW,
    PD_RECEIVER_SESSION_TOKEN_MISMATCH,
    PD_RECEIVER_SESSION_EXPIRED_RESULT
} pd_receiver_session_result;

typedef bool (*pd_random_fill_fn)(void *context, uint8_t *buffer, size_t buffer_size);

typedef struct pd_receiver_session {
    char token[PD_SESSION_TOKEN_CAPACITY];
    uint64_t created_at_ms;
    uint64_t expires_at_ms;
    pd_receiver_session_state state;
} pd_receiver_session;

pd_receiver_session_result pd_receiver_session_create(pd_receiver_session *session,
                                                      uint64_t now_ms,
                                                      uint64_t ttl_ms,
                                                      pd_random_fill_fn random_fill,
                                                      void *random_context);
bool pd_receiver_session_token_matches(const pd_receiver_session *session, const char *candidate);
bool pd_receiver_session_expire_if_due(pd_receiver_session *session, uint64_t now_ms);
pd_receiver_session_result pd_receiver_session_begin_transfer(pd_receiver_session *session,
                                                              const char *candidate_token,
                                                              uint64_t now_ms);
void pd_receiver_session_close(pd_receiver_session *session);

#endif
