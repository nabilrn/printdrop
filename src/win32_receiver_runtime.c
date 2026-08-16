#include "printdrop/win32_receiver_runtime.h"

#ifdef _WIN32

#include "printdrop/relay_endpoint.h"
#include "printdrop/win32_receive_handler.h"
#include "printdrop/win32_relay_client.h"
#include "printdrop/win32_relay_registration.h"

#include <string.h>

typedef struct pd_win32_runtime_io {
    pd_win32_relay_client *relay;
} pd_win32_runtime_io;

static bool pd_runtime_receive_frame(void *context,
                                     pd_frame_header *header,
                                     const uint8_t **payload,
                                     size_t *payload_size)
{
    pd_win32_runtime_io *io = (pd_win32_runtime_io *)context;

    for (;;) {
        pd_relay_message_view view;
        pd_relay_client_result result = pd_win32_relay_client_receive_frame(io->relay, &view);
        if (result == PD_RELAY_CLIENT_TIMEOUT) {
            continue;
        }
        if (result != PD_RELAY_CLIENT_OK) {
            return false;
        }

        *header = view.header;
        *payload = view.payload;
        *payload_size = view.payload_size;
        return true;
    }
}

static bool pd_runtime_send_frame(void *context,
                                  const pd_frame_header *header,
                                  const uint8_t *payload,
                                  size_t payload_size)
{
    pd_win32_runtime_io *io = (pd_win32_runtime_io *)context;
    return pd_win32_relay_client_send_frame(io->relay, header, payload, payload_size) ==
           PD_RELAY_CLIENT_OK;
}

static uint64_t pd_runtime_progress(void *context)
{
    pd_win32_receive_handler *handler = (pd_win32_receive_handler *)context;
    return handler->receiver.received_bytes;
}

pd_win32_receiver_runtime_result pd_win32_receiver_run_session(
    const char *relay_https_base_url,
    pd_receiver_session *session,
    const wchar_t *jobs_root_directory,
    pd_receiver_loop_event_fn event_callback,
    void *event_context)
{
    static const pd_receiver_loop_io_ops io_ops = {
        pd_runtime_receive_frame,
        pd_runtime_send_frame,
    };
    char registration_url[PD_RELAY_ENDPOINT_CAPACITY];
    char receiver_url[PD_RELAY_ENDPOINT_CAPACITY];
    pd_win32_relay_client relay;
    pd_win32_receive_handler handler;
    pd_receiver_protocol protocol;
    pd_receiver_loop loop;
    pd_win32_runtime_io io;
    pd_receiver_loop_result loop_result;

    if (relay_https_base_url == NULL || session == NULL || jobs_root_directory == NULL ||
        session->state != PD_RECEIVER_SESSION_WAITING || session->token[0] == '\0' ||
        session->receiver_secret[0] == '\0') {
        return PD_WIN32_RECEIVER_RUNTIME_INVALID_ARGUMENT;
    }

    if (pd_relay_build_registration_url(relay_https_base_url,
                                        registration_url,
                                        sizeof(registration_url)) != PD_RELAY_ENDPOINT_OK ||
        pd_relay_build_receiver_wss_url(relay_https_base_url,
                                        session->token,
                                        receiver_url,
                                        sizeof(receiver_url)) != PD_RELAY_ENDPOINT_OK) {
        return PD_WIN32_RECEIVER_RUNTIME_ENDPOINT_ERROR;
    }

    if (pd_win32_relay_register_session(registration_url,
                                        session->token,
                                        session->receiver_secret,
                                        NULL) != PD_RELAY_REGISTRATION_OK) {
        return PD_WIN32_RECEIVER_RUNTIME_REGISTRATION_ERROR;
    }

    memset(&relay, 0, sizeof(relay));
    if (pd_win32_relay_client_init(&relay,
                                   receiver_url,
                                   session->receiver_secret) != PD_RELAY_CLIENT_OK) {
        pd_receiver_session_close(session);
        return PD_WIN32_RECEIVER_RUNTIME_RELAY_ERROR;
    }

    if (pd_win32_relay_client_connect(&relay) != PD_RELAY_CLIENT_OK) {
        pd_win32_relay_client_cleanup(&relay);
        pd_receiver_session_close(session);
        return PD_WIN32_RECEIVER_RUNTIME_RELAY_ERROR;
    }

    if (pd_win32_receive_handler_init(&handler, jobs_root_directory) !=
        PD_WIN32_RECEIVE_HANDLER_OK) {
        pd_win32_relay_client_cleanup(&relay);
        pd_receiver_session_close(session);
        return PD_WIN32_RECEIVER_RUNTIME_STORAGE_ERROR;
    }

    if (pd_receiver_protocol_init(&protocol,
                                  pd_win32_receive_handler_ops(),
                                  &handler) != PD_RECEIVER_PROTOCOL_OK) {
        pd_win32_relay_client_cleanup(&relay);
        pd_receiver_session_close(session);
        return PD_WIN32_RECEIVER_RUNTIME_PROTOCOL_ERROR;
    }

    io.relay = &relay;
    memset(&loop, 0, sizeof(loop));
    loop.io_ops = &io_ops;
    loop.io_context = &io;
    loop.protocol = &protocol;
    loop.progress = pd_runtime_progress;
    loop.progress_context = &handler;
    loop.event_callback = event_callback;
    loop.event_context = event_context;

    loop_result = pd_receiver_loop_run(&loop);
    pd_win32_relay_client_cleanup(&relay);
    pd_receiver_session_close(session);

    if (loop_result == PD_RECEIVER_LOOP_OK) {
        return PD_WIN32_RECEIVER_RUNTIME_OK;
    }
    if (loop_result == PD_RECEIVER_LOOP_RECEIVE_ERROR ||
        loop_result == PD_RECEIVER_LOOP_SEND_ERROR) {
        return PD_WIN32_RECEIVER_RUNTIME_RELAY_ERROR;
    }
    return PD_WIN32_RECEIVER_RUNTIME_PROTOCOL_ERROR;
}

#endif
