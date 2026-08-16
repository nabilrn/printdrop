#ifndef PRINTDROP_WIN32_SHA256_H
#define PRINTDROP_WIN32_SHA256_H

#ifdef _WIN32

#include "printdrop/integrity.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct pd_win32_sha256 {
    void *algorithm_handle;
    void *hash_handle;
    uint8_t *object_buffer;
    unsigned long object_length;
    bool active;
} pd_win32_sha256;

void pd_win32_sha256_init(pd_win32_sha256 *context);
const pd_integrity_ops *pd_win32_sha256_ops(void);

#endif

#endif
