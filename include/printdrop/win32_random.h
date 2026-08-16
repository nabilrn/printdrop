#ifndef PRINTDROP_WIN32_RANDOM_H
#define PRINTDROP_WIN32_RANDOM_H

#ifdef _WIN32

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool pd_win32_random_fill(void *context, uint8_t *buffer, size_t buffer_size);

#endif

#endif
