#include "printdrop/win32_sha256.h"

#ifdef _WIN32

#include <windows.h>
#include <bcrypt.h>

#include <limits.h>
#include <stddef.h>
#include <string.h>

static void pd_win32_sha256_cleanup(pd_win32_sha256 *context)
{
    if (context == NULL) {
        return;
    }

    if (context->hash_handle != NULL) {
        BCryptDestroyHash((BCRYPT_HASH_HANDLE)context->hash_handle);
        context->hash_handle = NULL;
    }
    if (context->object_buffer != NULL) {
        HeapFree(GetProcessHeap(), 0U, context->object_buffer);
        context->object_buffer = NULL;
    }
    if (context->algorithm_handle != NULL) {
        BCryptCloseAlgorithmProvider((BCRYPT_ALG_HANDLE)context->algorithm_handle, 0U);
        context->algorithm_handle = NULL;
    }
    context->object_length = 0UL;
    context->active = false;
}

void pd_win32_sha256_init(pd_win32_sha256 *context)
{
    if (context != NULL) {
        memset(context, 0, sizeof(*context));
    }
}

static pd_integrity_status pd_win32_sha256_begin(void *opaque)
{
    pd_win32_sha256 *context = (pd_win32_sha256 *)opaque;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    DWORD object_length = 0U;
    DWORD result_length = 0U;
    uint8_t *object_buffer;

    if (context == NULL || context->active) {
        return PD_INTEGRITY_ERROR;
    }

    pd_win32_sha256_cleanup(context);

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0U) < 0) {
        return PD_INTEGRITY_ERROR;
    }

    if (BCryptGetProperty(algorithm,
                          BCRYPT_OBJECT_LENGTH,
                          (PUCHAR)&object_length,
                          (ULONG)sizeof(object_length),
                          &result_length,
                          0U) < 0 ||
        result_length != (DWORD)sizeof(object_length) || object_length == 0U) {
        BCryptCloseAlgorithmProvider(algorithm, 0U);
        return PD_INTEGRITY_ERROR;
    }

    object_buffer = (uint8_t *)HeapAlloc(GetProcessHeap(), 0U, (SIZE_T)object_length);
    if (object_buffer == NULL) {
        BCryptCloseAlgorithmProvider(algorithm, 0U);
        return PD_INTEGRITY_ERROR;
    }

    if (BCryptCreateHash(algorithm,
                         &hash,
                         object_buffer,
                         object_length,
                         NULL,
                         0U,
                         0U) < 0) {
        HeapFree(GetProcessHeap(), 0U, object_buffer);
        BCryptCloseAlgorithmProvider(algorithm, 0U);
        return PD_INTEGRITY_ERROR;
    }

    context->algorithm_handle = algorithm;
    context->hash_handle = hash;
    context->object_buffer = object_buffer;
    context->object_length = (unsigned long)object_length;
    context->active = true;
    return PD_INTEGRITY_OK;
}

static pd_integrity_status pd_win32_sha256_update(void *opaque,
                                                  const uint8_t *data,
                                                  size_t data_size)
{
    pd_win32_sha256 *context = (pd_win32_sha256 *)opaque;

    if (context == NULL || !context->active || context->hash_handle == NULL ||
        (data == NULL && data_size != 0U) || data_size > (size_t)ULONG_MAX) {
        return PD_INTEGRITY_ERROR;
    }

    if (BCryptHashData((BCRYPT_HASH_HANDLE)context->hash_handle,
                       (PUCHAR)data,
                       (ULONG)data_size,
                       0U) < 0) {
        return PD_INTEGRITY_ERROR;
    }
    return PD_INTEGRITY_OK;
}

static pd_integrity_status pd_win32_sha256_finish(void *opaque,
                                                  uint8_t digest[PD_SHA256_BYTES])
{
    pd_win32_sha256 *context = (pd_win32_sha256 *)opaque;
    NTSTATUS status;

    if (context == NULL || digest == NULL || !context->active || context->hash_handle == NULL) {
        return PD_INTEGRITY_ERROR;
    }

    status = BCryptFinishHash((BCRYPT_HASH_HANDLE)context->hash_handle,
                              digest,
                              (ULONG)PD_SHA256_BYTES,
                              0U);
    pd_win32_sha256_cleanup(context);
    return status < 0 ? PD_INTEGRITY_ERROR : PD_INTEGRITY_OK;
}

static void pd_win32_sha256_abort(void *opaque)
{
    pd_win32_sha256_cleanup((pd_win32_sha256 *)opaque);
}

const pd_integrity_ops *pd_win32_sha256_ops(void)
{
    static const pd_integrity_ops ops = {
        pd_win32_sha256_begin,
        pd_win32_sha256_update,
        pd_win32_sha256_finish,
        pd_win32_sha256_abort,
    };
    return &ops;
}

#endif
