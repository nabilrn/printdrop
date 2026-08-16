#include "printdrop/receiver_session.h"

#include "printdrop/secure_zero.h"

#include <limits.h>
#include <string.h>

#define PD_SESSION_ENTROPY_BYTES (PD_SESSION_TOKEN_BYTES + PD_RECEIVER_SECRET_BYTES)

static char pd_hex_digit(uint8_t value)
{
    static const char digits[] = "0123456789abcdef";
    return digits[value & UINT8_C(0x0f)];
}

static void pd_encode_hex(const uint8_t *input, size_t input_size, char *output)
{
    size_t index;

    for (index = 0U; index < input_size; ++index) {
        output[index * 2U] = pd_hex_digit((uint8_t)(input[index] >> 4U));
        output[(index * 2U) + 1U] = pd_hex_digit(input[index]);
    }
    output[input_size * 2U] = '\0';
}

static bool pd_constant_time_text_equal(const char *expected,
                                        size_t expected_length,
                                        const char *candidate)
{
    size_t index;
    unsigned int difference = 0U;

    if (candidate == NULL || strlen(candidate) != expected_length) {
        return false;
    }

    for (index = 0U; index < expected_length; ++index) {
        difference |= (unsigned int)((unsigned char)expected[index] ^
                                     (unsigned char)candidate[index]);
    }
    return difference == 0U;
}

static void pd_receiver_session_reset(pd_receiver_session *session)
{
    pd_secure_zero(session, sizeof(*session));
    session->state = PD_RECEIVER_SESSION_INACTIVE;
}

static void pd_receiver_session_clear_credentials(pd_receiver_session *session)
{
    pd_secure_zero(session->token, sizeof(session->token));
    pd_secure_zero(session->receiver_secret, sizeof(session->receiver_secret));
}

pd_receiver_session_result pd_receiver_session_create(pd_receiver_session *session,
                                                      uint64_t now_ms,
                                                      uint64_t ttl_ms,
                                                      pd_random_fill_fn random_fill,
                                                      void *random_context)
{
    uint8_t entropy[PD_SESSION_ENTROPY_BYTES];

    if (session == NULL || random_fill == NULL || ttl_ms == UINT64_C(0)) {
        return PD_RECEIVER_SESSION_INVALID_ARGUMENT;
    }

    pd_receiver_session_reset(session);

    if (ttl_ms > UINT64_MAX - now_ms) {
        return PD_RECEIVER_SESSION_CLOCK_OVERFLOW;
    }

    if (!random_fill(random_context, entropy, sizeof(entropy))) {
        pd_secure_zero(entropy, sizeof(entropy));
        return PD_RECEIVER_SESSION_ENTROPY_FAILURE;
    }

    pd_encode_hex(entropy, (size_t)PD_SESSION_TOKEN_BYTES, session->token);
    pd_encode_hex(&entropy[PD_SESSION_TOKEN_BYTES],
                  (size_t)PD_RECEIVER_SECRET_BYTES,
                  session->receiver_secret);
    pd_secure_zero(entropy, sizeof(entropy));

    session->created_at_ms = now_ms;
    session->expires_at_ms = now_ms + ttl_ms;
    session->state = PD_RECEIVER_SESSION_WAITING;
    return PD_RECEIVER_SESSION_OK;
}

bool pd_receiver_session_token_matches(const pd_receiver_session *session, const char *candidate)
{
    if (session == NULL) {
        return false;
    }
    return pd_constant_time_text_equal(session->token,
                                       (size_t)PD_SESSION_TOKEN_HEX_CHARS,
                                       candidate);
}

bool pd_receiver_session_secret_matches(const pd_receiver_session *session, const char *candidate)
{
    if (session == NULL) {
        return false;
    }
    return pd_constant_time_text_equal(session->receiver_secret,
                                       (size_t)PD_RECEIVER_SECRET_HEX_CHARS,
                                       candidate);
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

    pd_receiver_session_clear_credentials(session);
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

    pd_receiver_session_clear_credentials(session);
    session->state = PD_RECEIVER_SESSION_CLOSED;
}
