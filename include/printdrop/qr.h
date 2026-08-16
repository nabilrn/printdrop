#ifndef PRINTDROP_QR_H
#define PRINTDROP_QR_H

#include <stdbool.h>
#include <stdint.h>

#define PD_QR_MAX_VERSION 10
#define PD_QR_STORAGE_CAPACITY 512U

typedef enum pd_qr_result {
    PD_QR_OK = 0,
    PD_QR_INVALID_ARGUMENT,
    PD_QR_DATA_TOO_LONG
} pd_qr_result;

typedef struct pd_qr_code {
    uint8_t encoded[PD_QR_STORAGE_CAPACITY];
    int size;
} pd_qr_code;

pd_qr_result pd_qr_encode_text(pd_qr_code *code, const char *text);
int pd_qr_size(const pd_qr_code *code);
bool pd_qr_get_module(const pd_qr_code *code, int x, int y);

#endif
