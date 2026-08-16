#include "printdrop/filename.h"

#include <stdbool.h>
#include <string.h>

static bool pd_ascii_is_control(unsigned char value)
{
    return value < 0x20U || value == 0x7fU;
}

static bool pd_is_windows_forbidden(unsigned char value)
{
    return value == (unsigned char)'<' || value == (unsigned char)'>' ||
           value == (unsigned char)':' || value == (unsigned char)'"' ||
           value == (unsigned char)'/' || value == (unsigned char)'\\' ||
           value == (unsigned char)'|' || value == (unsigned char)'?' ||
           value == (unsigned char)'*';
}

static unsigned char pd_ascii_upper(unsigned char value)
{
    if (value >= (unsigned char)'a' && value <= (unsigned char)'z') {
        return (unsigned char)(value - ((unsigned char)'a' - (unsigned char)'A'));
    }
    return value;
}

static bool pd_ascii_equal_prefix(const char *value, size_t value_length, const char *literal)
{
    size_t index;
    size_t literal_length = strlen(literal);

    if (value_length != literal_length) {
        return false;
    }

    for (index = 0U; index < value_length; ++index) {
        if (pd_ascii_upper((unsigned char)value[index]) != (unsigned char)literal[index]) {
            return false;
        }
    }

    return true;
}

static bool pd_is_windows_reserved_device_name(const char *filename, size_t filename_length)
{
    size_t base_length = 0U;

    while (base_length < filename_length && filename[base_length] != '.') {
        base_length += 1U;
    }

    if (pd_ascii_equal_prefix(filename, base_length, "CON") ||
        pd_ascii_equal_prefix(filename, base_length, "PRN") ||
        pd_ascii_equal_prefix(filename, base_length, "AUX") ||
        pd_ascii_equal_prefix(filename, base_length, "NUL")) {
        return true;
    }

    if (base_length == 4U) {
        unsigned char prefix0 = pd_ascii_upper((unsigned char)filename[0]);
        unsigned char prefix1 = pd_ascii_upper((unsigned char)filename[1]);
        unsigned char prefix2 = pd_ascii_upper((unsigned char)filename[2]);
        unsigned char digit = (unsigned char)filename[3];
        bool is_com = prefix0 == (unsigned char)'C' && prefix1 == (unsigned char)'O' &&
                      prefix2 == (unsigned char)'M';
        bool is_lpt = prefix0 == (unsigned char)'L' && prefix1 == (unsigned char)'P' &&
                      prefix2 == (unsigned char)'T';

        if ((is_com || is_lpt) && digit >= (unsigned char)'1' && digit <= (unsigned char)'9') {
            return true;
        }
    }

    return false;
}

pd_filename_result pd_filename_sanitize(const char *input,
                                        char *output,
                                        size_t output_capacity,
                                        size_t *required_capacity)
{
    size_t input_length;
    size_t source_index;
    size_t target_length = 0U;
    size_t prefix_length = 0U;
    char candidate[PD_FILENAME_CAPACITY];

    if (required_capacity != NULL) {
        *required_capacity = 0U;
    }

    if (input == NULL || output == NULL) {
        return PD_FILENAME_INVALID_ARGUMENT;
    }

    input_length = strlen(input);
    if (input_length > (size_t)PD_FILENAME_MAX_BYTES) {
        return PD_FILENAME_TOO_LONG;
    }

    for (source_index = 0U; source_index < input_length; ++source_index) {
        unsigned char value = (unsigned char)input[source_index];

        if (pd_ascii_is_control(value) || pd_is_windows_forbidden(value)) {
            candidate[target_length] = '_';
        } else {
            candidate[target_length] = (char)value;
        }
        target_length += 1U;
    }

    while (target_length > 0U &&
           (candidate[target_length - 1U] == ' ' || candidate[target_length - 1U] == '.')) {
        target_length -= 1U;
    }

    if (target_length == 0U ||
        (target_length == 1U && candidate[0] == '.') ||
        (target_length == 2U && candidate[0] == '.' && candidate[1] == '.')) {
        static const char fallback[] = "file";
        memcpy(candidate, fallback, sizeof(fallback) - 1U);
        target_length = sizeof(fallback) - 1U;
    }

    candidate[target_length] = '\0';

    if (pd_is_windows_reserved_device_name(candidate, target_length)) {
        prefix_length = 1U;
    }

    if (target_length > (size_t)PD_FILENAME_MAX_BYTES - prefix_length) {
        return PD_FILENAME_TOO_LONG;
    }

    if (required_capacity != NULL) {
        *required_capacity = target_length + prefix_length + 1U;
    }

    if (output_capacity < target_length + prefix_length + 1U) {
        if (output_capacity > 0U) {
            output[0] = '\0';
        }
        return PD_FILENAME_BUFFER_TOO_SMALL;
    }

    if (prefix_length != 0U) {
        output[0] = '_';
    }
    memcpy(output + prefix_length, candidate, target_length);
    output[prefix_length + target_length] = '\0';
    return PD_FILENAME_OK;
}
