#include "printdrop/transport.h"

bool pd_transport_is_configured(const pd_transport *transport)
{
    return transport != NULL && transport->ops != NULL && transport->ops->open != NULL &&
           transport->ops->write != NULL && transport->ops->read != NULL &&
           transport->ops->close != NULL;
}

pd_transport_status pd_transport_open(pd_transport *transport)
{
    pd_transport_status status;

    if (!pd_transport_is_configured(transport)) {
        return PD_TRANSPORT_INVALID_ARGUMENT;
    }

    if (transport->is_open) {
        return PD_TRANSPORT_INVALID_STATE;
    }

    status = transport->ops->open(transport->context);
    if (status == PD_TRANSPORT_OK) {
        transport->is_open = true;
    }

    return status;
}

pd_transport_status pd_transport_write(pd_transport *transport,
                                       const uint8_t *buffer,
                                       size_t buffer_size,
                                       size_t *bytes_written)
{
    if (bytes_written == NULL || (buffer == NULL && buffer_size != 0U)) {
        return PD_TRANSPORT_INVALID_ARGUMENT;
    }

    *bytes_written = 0U;

    if (!pd_transport_is_configured(transport)) {
        return PD_TRANSPORT_INVALID_ARGUMENT;
    }

    if (!transport->is_open) {
        return PD_TRANSPORT_INVALID_STATE;
    }

    return transport->ops->write(transport->context, buffer, buffer_size, bytes_written);
}

pd_transport_status pd_transport_read(pd_transport *transport,
                                      uint8_t *buffer,
                                      size_t buffer_size,
                                      size_t *bytes_read)
{
    if (bytes_read == NULL || (buffer == NULL && buffer_size != 0U)) {
        return PD_TRANSPORT_INVALID_ARGUMENT;
    }

    *bytes_read = 0U;

    if (!pd_transport_is_configured(transport)) {
        return PD_TRANSPORT_INVALID_ARGUMENT;
    }

    if (!transport->is_open) {
        return PD_TRANSPORT_INVALID_STATE;
    }

    return transport->ops->read(transport->context, buffer, buffer_size, bytes_read);
}

void pd_transport_close(pd_transport *transport)
{
    if (!pd_transport_is_configured(transport) || !transport->is_open) {
        return;
    }

    transport->ops->close(transport->context);
    transport->is_open = false;
}
