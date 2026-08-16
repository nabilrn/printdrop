#ifndef PRINTDROP_INTEGRITY_H
#define PRINTDROP_INTEGRITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PD_SHA256_BYTES 32U
#define PD_SHA256_HEX_CHARS (PD_SHA256_BYTES * 2U)
#define PD_SHA256_HEX_CAPACITY (PD_SHA256_HEX_CHARS + 1U)

typedef enum pd_integrity_status {
    PD_INTEGRITY_OK = 0,
    PD_INTEGRITY_ERROR
} pd_integrity_status;

typedef struct pd_integrity_ops {
    pd_integrity_status (*begin)(void *context);
    pd_integrity_status (*update)(void *context, const uint8_t *data, size_t data_size);
    pd_integrity_status (*finish)(void *context, uint8_t digest[PD_SHA256_BYTES]);
    void (*abort)(void *context);
} pd_integrity_ops;

typedef enum pd_digest_result {
    PD_DIGEST_OK = 0,
    PD_DIGEST_INVALID_ARGUMENT,
    PD_DIGEST_INVALID_HEX
} pd_digest_result;

bool pd_digest_equal(const uint8_t left[PD_SHA256_BYTES],
                     const uint8_t right[PD_SHA256_BYTES]);
pd_digest_result pd_sha256_from_hex(const char *hex, uint8_t digest[PD_SHA256_BYTES]);
void pd_sha256_to_hex(const uint8_t digest[PD_SHA256_BYTES],
                      char output[PD_SHA256_HEX_CAPACITY]);

#endif
