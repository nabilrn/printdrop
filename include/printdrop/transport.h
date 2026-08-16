#ifndef PRINTDROP_TRANSPORT_H
#define PRINTDROP_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum pd_transport_status {
    PD_TRANSPORT_OK = 0,
    PD_TRANSPORT_INVALID_ARGUMENT,
    PD_TRANSPORT_INVALID_STATE,
    PD_TRANSPORT_IO_ERROR,
    PD_TRANSPORT_WOULD_BLOCK,
    PD_TRANSPORT_CLOSED
} pd_transport_status;

typedef struct pd_transport_ops {
    pd_transport_status (*open)(void *context);
    pd_transport_status (*write)(void *context,
                                 const uint8_t *buffer,
                                 size_t buffer_size,
                                 size_t *bytes_written);
    pd_transport_status (*read)(void *context,
                                uint8_t *buffer,
                                size_t buffer_size,
                                size_t *bytes_read);
    void (*close)(void *context);
} pd_transport_ops;

typedef struct pd_transport {
    const pd_transport_ops *ops;
    void *context;
    bool is_open;
} pd_transport;

bool pd_transport_is_configured(const pd_transport *transport);
pd_transport_status pd_transport_open(pd_transport *transport);
pd_transport_status pd_transport_write(pd_transport *transport,
                                       const uint8_t *buffer,
                                       size_t buffer_size,
                                       size_t *bytes_written);
pd_transport_status pd_transport_read(pd_transport *transport,
                                      uint8_t *buffer,
                                      size_t buffer_size,
                                      size_t *bytes_read);
void pd_transport_close(pd_transport *transport);

#endif
