#ifndef PRINTDROP_PROTOCOL_H
#define PRINTDROP_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define PD_PROTOCOL_VERSION UINT8_C(1)

typedef enum pd_message_type {
    PD_MSG_HELLO = 1,
    PD_MSG_JOB = 2,
    PD_MSG_FILE_BEGIN = 3,
    PD_MSG_CHUNK = 4,
    PD_MSG_FILE_END = 5,
    PD_MSG_ACK = 6,
    PD_MSG_ERROR = 7
} pd_message_type;

bool pd_message_type_is_valid(uint8_t value);
const char *pd_message_type_name(pd_message_type type);

#endif
