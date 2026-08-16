#include "printdrop/qr.h"

#include "qrcodegen.h"

#include <string.h>

_Static_assert(PD_QR_STORAGE_CAPACITY >= qrcodegen_BUFFER_LEN_FOR_VERSION(PD_QR_MAX_VERSION),
               "PrintDrop QR storage is too small for the configured maximum QR version");

pd_qr_result pd_qr_encode_text(pd_qr_code *code, const char *text)
{
    uint8_t temp[PD_QR_STORAGE_CAPACITY];
    bool success;

    if (code == NULL || text == NULL || text[0] == '\0') {
        return PD_QR_INVALID_ARGUMENT;
    }

    memset(code, 0, sizeof(*code));
    success = qrcodegen_encodeText(text,
                                   temp,
                                   code->encoded,
                                   qrcodegen_Ecc_MEDIUM,
                                   qrcodegen_VERSION_MIN,
                                   PD_QR_MAX_VERSION,
                                   qrcodegen_Mask_AUTO,
                                   true);
    if (!success) {
        return PD_QR_DATA_TOO_LONG;
    }

    code->size = qrcodegen_getSize(code->encoded);
    return PD_QR_OK;
}

int pd_qr_size(const pd_qr_code *code)
{
    return code == NULL ? 0 : code->size;
}

bool pd_qr_get_module(const pd_qr_code *code, int x, int y)
{
    if (code == NULL || code->size <= 0) {
        return false;
    }

    return qrcodegen_getModule(code->encoded, x, y);
}
