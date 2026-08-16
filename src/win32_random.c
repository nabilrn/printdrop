#include "printdrop/win32_random.h"

#ifdef _WIN32

#include <windows.h>
#include <bcrypt.h>

#include <limits.h>

bool pd_win32_random_fill(void *context, uint8_t *buffer, size_t buffer_size)
{
    (void)context;

    if ((buffer == NULL && buffer_size != 0U) || buffer_size > (size_t)ULONG_MAX) {
        return false;
    }

    if (buffer_size == 0U) {
        return true;
    }

    return BCryptGenRandom(NULL,
                           buffer,
                           (ULONG)buffer_size,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}

#endif
