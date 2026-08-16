#include "printdrop/ack.h"
#include "printdrop/file_begin.h"
#include "printdrop/receiver_loop.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define PD_TEST_ASSERT(condition)                                                                    \
    do {                                                                                             \
        if (!(condition)) {                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                    \
            failures += 1;                                                                           \
        }                                                                                            \
    } while (0)

typedef struct fake_handler {
    uint64_t received_bytes;
    unsigned int begin_calls;
    unsigned int finish_calls;
    unsigned int abort_calls;
    bool fail_begin;
} fake_handler;

typedef struct fake_input_frame {
    pd_frame_header header;
    const uint8_t *payload;
    size_t payload_size;
} fake_input_frame;

typedef struct fake_io {
    const fake_input_frame *frames;
    size_t frame_count;
    size_t next_frame;
    pd_ack acks[8];
    size_t ack_count;
    bool fail_send;
} fake_io;

typedef struct fake_events {
    pd_receiver_loop_event events[8];
    size_t count;
} fake_events;

static bool fake_begin_file(void *context,
                            const pd_file_begin *metadata,
                            const char *sanitized_filename)
{
    fake_handler *handler = (fake_handler *)context;
    handler->begin_calls += 1U;
    PD_TEST_ASSERT(metadata->file_size == UINT64_C(3));
    PD_TEST_ASSERT(strcmp(sanitized_filename, "hello.txt") == 0);
    return !handler->fail_begin;
}

static bool fake_write_chunk(void *context, const uint8_t *data, size_t data_size)
{
    fake_handler *handler = (fake_handler *)context;
    PD_TEST_ASSERT(data != NULL);
    handler->received_bytes += (uint64_t)data_size;
    return true;
}

static bool fake_finish_file(void *context)
{
    fake_handler *handler = (fake_handler *)context;
    handler->finish_calls += 1U;
    return handler->received_bytes == UINT64_C(3);
}

static void fake_abort_file(void *context)
{
    fake_handler *handler = (fake_handler *)context;
    handler->abort_calls += 1U;
}

static bool fake_receive_frame(void *context,
                               pd_frame_header *header,
                               const uint8_t **payload,
                               size_t *payload_size)
{
    fake_io *io = (fake_io *)context;
    const fake_input_frame *frame;

    if (io->next_frame >= io->frame_count) {
        return false;
    }

    frame = &io->frames[io->next_frame++];
    *header = frame->header;
    *payload = frame->payload;
    *payload_size = frame->payload_size;
    return true;
}

static bool fake_send_frame(void *context,
                            const pd_frame_header *header,
                            const uint8_t *payload,
                            size_t payload_size)
{
    fake_io *io = (fake_io *)context;
    pd_ack ack;

    if (io->fail_send) {
        return false;
    }
    PD_TEST_ASSERT(header->type == PD_MSG_ACK);
    PD_TEST_ASSERT(header->payload_length == (uint32_t)PD_ACK_PAYLOAD_SIZE);
    PD_TEST_ASSERT(payload_size == (size_t)PD_ACK_PAYLOAD_SIZE);
    if (io->ack_count >= sizeof(io->acks) / sizeof(io->acks[0])) {
        return false;
    }
    if (pd_ack_decode(payload, payload_size, &ack) != PD_ACK_RESULT_OK) {
        return false;
    }
    io->acks[io->ack_count++] = ack;
    return true;
}

static uint64_t fake_progress(void *context)
{
    return ((fake_handler *)context)->received_bytes;
}

static void fake_event(void *context, pd_receiver_loop_event event, uint64_t received_bytes)
{
    fake_events *events = (fake_events *)context;
    (void)received_bytes;
    if (events->count < sizeof(events->events) / sizeof(events->events[0])) {
        events->events[events->count++] = event;
    }
}

static size_t make_file_begin(uint8_t *output, size_t capacity)
{
    pd_file_begin metadata;
    size_t written = 0U;

    memset(&metadata, 0, sizeof(metadata));
    metadata.file_size = UINT64_C(3);
    memcpy(metadata.filename, "hello.txt", sizeof("hello.txt"));
    PD_TEST_ASSERT(pd_file_begin_encode(&metadata, output, capacity, &written) == PD_FILE_BEGIN_OK);
    return written;
}

static void test_happy_path(void)
{
    static const pd_receiver_protocol_ops handler_ops = {
        fake_begin_file,
        fake_write_chunk,
        fake_finish_file,
        fake_abort_file,
    };
    static const pd_receiver_loop_io_ops io_ops = {
        fake_receive_frame,
        fake_send_frame,
    };
    static const uint8_t chunk[] = {'a', 'b', 'c'};
    uint8_t begin_payload[PD_FILE_BEGIN_MAX_PAYLOAD];
    size_t begin_size = make_file_begin(begin_payload, sizeof(begin_payload));
    fake_input_frame frames[3];
    fake_handler handler = {0};
    fake_io io = {0};
    fake_events events = {0};
    pd_receiver_protocol protocol;
    pd_receiver_loop loop;

    frames[0].header.type = PD_MSG_FILE_BEGIN;
    frames[0].header.flags = UINT16_C(0);
    frames[0].header.payload_length = (uint32_t)begin_size;
    frames[0].payload = begin_payload;
    frames[0].payload_size = begin_size;
    frames[1].header.type = PD_MSG_CHUNK;
    frames[1].header.flags = UINT16_C(0);
    frames[1].header.payload_length = (uint32_t)sizeof(chunk);
    frames[1].payload = chunk;
    frames[1].payload_size = sizeof(chunk);
    frames[2].header.type = PD_MSG_FILE_END;
    frames[2].header.flags = UINT16_C(0);
    frames[2].header.payload_length = UINT32_C(0);
    frames[2].payload = NULL;
    frames[2].payload_size = 0U;

    io.frames = frames;
    io.frame_count = 3U;
    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol, &handler_ops, &handler) ==
                   PD_RECEIVER_PROTOCOL_OK);

    memset(&loop, 0, sizeof(loop));
    loop.io_ops = &io_ops;
    loop.io_context = &io;
    loop.protocol = &protocol;
    loop.progress = fake_progress;
    loop.progress_context = &handler;
    loop.event_callback = fake_event;
    loop.event_context = &events;

    PD_TEST_ASSERT(pd_receiver_loop_run(&loop) == PD_RECEIVER_LOOP_OK);
    PD_TEST_ASSERT(handler.begin_calls == 1U);
    PD_TEST_ASSERT(handler.finish_calls == 1U);
    PD_TEST_ASSERT(handler.abort_calls == 0U);
    PD_TEST_ASSERT(handler.received_bytes == UINT64_C(3));
    PD_TEST_ASSERT(io.ack_count == 3U);
    PD_TEST_ASSERT(io.acks[0].acknowledged_type == PD_MSG_FILE_BEGIN);
    PD_TEST_ASSERT(io.acks[0].received_bytes == UINT64_C(0));
    PD_TEST_ASSERT(io.acks[1].acknowledged_type == PD_MSG_CHUNK);
    PD_TEST_ASSERT(io.acks[1].received_bytes == UINT64_C(3));
    PD_TEST_ASSERT(io.acks[2].acknowledged_type == PD_MSG_FILE_END);
    PD_TEST_ASSERT(io.acks[2].received_bytes == UINT64_C(3));
    PD_TEST_ASSERT(events.count == 5U);
    PD_TEST_ASSERT(events.events[0] == PD_RECEIVER_LOOP_READY);
    PD_TEST_ASSERT(events.events[1] == PD_RECEIVER_LOOP_FILE_STARTED);
    PD_TEST_ASSERT(events.events[2] == PD_RECEIVER_LOOP_PROGRESS);
    PD_TEST_ASSERT(events.events[3] == PD_RECEIVER_LOOP_VERIFYING);
    PD_TEST_ASSERT(events.events[4] == PD_RECEIVER_LOOP_COMPLETE);
}

static void test_unexpected_message_fails_closed(void)
{
    static const pd_receiver_protocol_ops handler_ops = {
        fake_begin_file,
        fake_write_chunk,
        fake_finish_file,
        fake_abort_file,
    };
    static const pd_receiver_loop_io_ops io_ops = {
        fake_receive_frame,
        fake_send_frame,
    };
    static const uint8_t chunk[] = {'x'};
    fake_input_frame frame;
    fake_handler handler = {0};
    fake_io io = {0};
    fake_events events = {0};
    pd_receiver_protocol protocol;
    pd_receiver_loop loop;

    frame.header.type = PD_MSG_CHUNK;
    frame.header.flags = UINT16_C(0);
    frame.header.payload_length = UINT32_C(1);
    frame.payload = chunk;
    frame.payload_size = sizeof(chunk);
    io.frames = &frame;
    io.frame_count = 1U;

    PD_TEST_ASSERT(pd_receiver_protocol_init(&protocol, &handler_ops, &handler) ==
                   PD_RECEIVER_PROTOCOL_OK);
    memset(&loop, 0, sizeof(loop));
    loop.io_ops = &io_ops;
    loop.io_context = &io;
    loop.protocol = &protocol;
    loop.progress = fake_progress;
    loop.progress_context = &handler;
    loop.event_callback = fake_event;
    loop.event_context = &events;

    PD_TEST_ASSERT(pd_receiver_loop_run(&loop) == PD_RECEIVER_LOOP_PROTOCOL_ERROR);
    PD_TEST_ASSERT(protocol.state == PD_RECEIVER_PROTOCOL_FAILED);
    PD_TEST_ASSERT(io.ack_count == 1U);
    PD_TEST_ASSERT(io.acks[0].acknowledged_type == PD_MSG_CHUNK);
    PD_TEST_ASSERT(io.acks[0].status == PD_ACK_PROTOCOL_ERROR);
    PD_TEST_ASSERT(events.count == 2U);
    PD_TEST_ASSERT(events.events[0] == PD_RECEIVER_LOOP_READY);
    PD_TEST_ASSERT(events.events[1] == PD_RECEIVER_LOOP_FAILED);
}

int main(void)
{
    test_happy_path();
    test_unexpected_message_fails_closed();

    if (failures != 0) {
        fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }

    puts("Receiver loop tests passed.");
    return 0;
}
