#include "printdrop/protocol.h"

bool pd_message_type_is_valid(uint8_t value)
{
    return value >= (uint8_t)PD_MSG_HELLO && value <= (uint8_t)PD_MSG_ERROR;
}

const char *pd_message_type_name(pd_message_type type)
{
    switch (type) {
    case PD_MSG_HELLO:
        return "HELLO";
    case PD_MSG_JOB:
        return "JOB";
    case PD_MSG_FILE_BEGIN:
        return "FILE_BEGIN";
    case PD_MSG_CHUNK:
        return "CHUNK";
    case PD_MSG_FILE_END:
        return "FILE_END";
    case PD_MSG_ACK:
        return "ACK";
    case PD_MSG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
