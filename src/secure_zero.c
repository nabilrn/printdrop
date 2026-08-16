#include "printdrop/secure_zero.h"

void pd_secure_zero(void *buffer, size_t buffer_size)
{
    volatile unsigned char *cursor = (volatile unsigned char *)buffer;

    if (buffer == NULL) {
        return;
    }

    while (buffer_size > 0U) {
        *cursor = 0U;
        cursor += 1;
        buffer_size -= 1U;
    }
}
