#include "printdrop/integrity.h"

#include <string.h>

static int pd_hex_value(char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return 10 + (character - 'a');
    }
    if (character >= 'A' && character <= 'F') {
        return 10 + (character - 'A');
    }
    return -1;
}

bool pd_digest_equal(const uint8_t left[PD_SHA256_BYTES],
                     const uint8_t right[PD_SHA256_BYTES])
{
    size_t index;
    uint8_t difference = UINT8_C(0);

    if (left == NULL || right == NULL) {
        return false;
    }

    for (index = 0U; index < (size_t)PD_SHA256_BYTES; ++index) {
        difference = (uint8_t)(difference | (uint8_t)(left[index] ^ right[index]));
    }
    return difference == UINT8_C(0);
}

pd_digest_result pd_sha256_from_hex(const char *hex, uint8_t digest[PD_SHA256_BYTES])
{
    size_t index;

    if (hex == NULL || digest == NULL) {
        return PD_DIGEST_INVALID_ARGUMENT;
    }

    if (strlen(hex) != (size_t)PD_SHA256_HEX_CHARS) {
        return PD_DIGEST_INVALID_HEX;
    }

    for (index = 0U; index < (size_t)PD_SHA256_BYTES; ++index) {
        int high = pd_hex_value(hex[index * 2U]);
        int low = pd_hex_value(hex[(index * 2U) + 1U]);
        if (high < 0 || low < 0) {
            return PD_DIGEST_INVALID_HEX;
        }
        digest[index] = (uint8_t)(((unsigned int)high << 4U) | (unsigned int)low);
    }
    return PD_DIGEST_OK;
}

void pd_sha256_to_hex(const uint8_t digest[PD_SHA256_BYTES],
                      char output[PD_SHA256_HEX_CAPACITY])
{
    static const char digits[] = "0123456789abcdef";
    size_t index;

    if (digest == NULL || output == NULL) {
        return;
    }

    for (index = 0U; index < (size_t)PD_SHA256_BYTES; ++index) {
        output[index * 2U] = digits[digest[index] >> 4U];
        output[(index * 2U) + 1U] = digits[digest[index] & UINT8_C(0x0f)];
    }
    output[PD_SHA256_HEX_CHARS] = '\0';
}
