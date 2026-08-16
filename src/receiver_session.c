#include "printdrop/receiver_session.h"

#include <limits.h>
#include <string.h>

static char pd_hex_digit(uint8_t value)
{
    static const char digits[] = "0123456789abcdef";
    return digits[value & UINT8_C(0x0f)];
}

static void pd_receiver_session_reset(pd_receiver_session *session)
{
    memset(session, 0, sizeof(*session));
    session->state = PD_RECEIVER_SESSION_INACTIVE;
}

pd_receiver_session_result pd_receiver_session_create(pd_receiver_session *session,
                                                      uint64_t now_ms,
                                                      uint64_t ttl_ms,
                                                      pd_random_fill_fn random_fill,
                                                      void *random_context)
{
    uint8_t random_bytes[PD_SESSION_TOKEN_BYTES];
    size_t index;

    if (session == NULL || random_fill == NULL || ttl_ms == UINT64_C(0)) {
        return PD_RECEIVER_SESSION_INVALID_ARGUMENT;
    }

    pd_receiver_session_reset(session);

    if (ttl_ms > UINT64_MAX - now_ms) {
        return PD_RECEIVER_SESSION_CLOCK_OVERFLOW;
    }

    if (!random_fill(random_context, random_bytes, sizeof(random_bytes))) {
        return PD_RECEIVER_SESSION_ENTROPY_FAILURE;
    }

    for (index = 0U; index < sizeof(random_bytes); ++index) {
        session->token[index * 2U] = pd_hex_digit((uint8_t)(random_bytes[index] >> 4U));
        session->token[(index * 2U) + 1U] = pd_hex_digit(random_bytes[index]);
    }

    session->token[PD_SESSION_TOKEN_HEX_CHARS] = '\0';
    session->created_at_ms = now_ms;
    session->expires_at_ms = now_ms + ttl_ms;
    session->state = PD_RECEIVER_SESSION_WAITING;
    return PD_RECEIVER_SESSION_OK;
}

bool pd_receiver_session_token_matches(const pd_receiver_session *session, const char *candidate)
{
    size_t index;
    unsigned int difference = 0U;

    if (session == NULL || candidate == NULL ||
        strlen(candidate) != (size_t)PD_SESSION_TOKEN_HEX_CHARS) {
        return false;
    }

    for (index = 0U; index < (size_t)PD_SESSION_TOKEN_HEX_CHARS; ++index) {
        difference |= (unsigned int)((unsigned char)session->token[index] ^
                                     (unsigned char)candidate[index]);
    }

    return difference == 0U;
}

bool pd_receiver_session_expire_if_due(pd_receiver_session *session, uint64_t now_ms)
{
    if (session == NULL || session->state == PD_RECEIVER_SESSION_INACTIVE ||
        session->state == PD_RECEIVER_SESSION_CLOSED ||
        session->state == PD_RECEIVER_SESSION_EXPIRED) {
        return false;
    }

    if (now_ms < session->expires_at_ms) {
        return false;
    }

    session->state = PD_RECEIVER_SESSION_EXPIRED;
    return true;
}

pd_receiver_session_result pd_receiver_session_begin_transfer(pd_receiver_session *session,
                                                              const char *candidate_token,
                                                              uint64_t now_ms)
{
    if (session == NULL || candidate_token == NULL) {
        return PD_RECEIVER_SESSION_INVALID_ARGUMENT;
    }

    if (session->state != PD_RECEIVER_SESSION_WAITING) {
        return session->state == PD_RECEIVER_SESSION_EXPIRED
                   ? PD_RECEIVER_SESSION_EXPIRED_RESULT
                   : PD_RECEIVER_SESSION_INVALID_STATE;
    }

    if (pd_receiver_session_expire_if_due(session, now_ms)) {
        return PD_RECEIVER_SESSION_EXPIRED_RESULT;
    }

    if (!pd_receiver_session_token_matches(session, candidate_token)) {
        return PD_RECEIVER_SESSION_TOKEN_MISMATCH;
    }

    session->state = PD_RECEIVER_SESSION_TRANSFERRING;
    return PD_RECEIVER_SESSION_OK;
}

void pd_receiver_session_close(pd_receiver_session *session)
{
    if (session == NULL || session->state == PD_RECEIVER_SESSION_INACTIVE) {
        return;
    }

    session->state = PD_RECEIVER_SESSION_CLOSED;
    memset(session->token, 0, sizeof(session->token));
}
