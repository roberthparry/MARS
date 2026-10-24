/**
 * @file http_receive.c
 * @brief Bounded response storage for asynchronous host HTTP transports.
 *
 * Browser Fetch and other event-driven adapters reserve writable chunks directly in this owner's memory.
 * Completion makes the body available to native protocol decoders; failure never exposes a partial body.
 * This portable component needs allocation only, not libcurl, sockets, threads or host networking imports.
 * Callers serialise operations and cancel the host operation before destroying its receiver.
 */
#include <stdlib.h>

#include "http-t.h"

struct _http_receive_t {
    unsigned char *data;
    size_t limit, capacity, used, reserved;
    unsigned status;
    http_error_t error;
};

/* Create an idle bounded response owner. */
http_receive_t *http_receive_new(size_t max_body_bytes)
{
    if (!max_body_bytes) return NULL;
    http_receive_t *receiver = calloc(1, sizeof *receiver);
    if (receiver) receiver->limit = max_body_bytes;
    return receiver;
}

/* Release all retained response storage. */
void http_receive_free(http_receive_t *receiver)
{
    if (!receiver) return;
    free(receiver->data);
    free(receiver);
}

/* An unsuccessful transfer never publishes its partial body. */
void http_receive_fail(http_receive_t *receiver, http_error_t error)
{
    if (!receiver || receiver->status || receiver->error) return;
    receiver->error = error == HTTP_ERROR_NONE ? HTTP_ERROR_TRANSPORT : error;
    receiver->reserved = 0;
}

/* Grow geometrically within the explicit decoded-body budget. */
unsigned char *http_receive_reserve(http_receive_t *receiver, size_t size)
{
    if (!receiver || receiver->status || receiver->error) return NULL;
    if (!size || receiver->reserved) {
        http_receive_fail(receiver, HTTP_ERROR_ARGUMENT); return NULL;
    }
    if (size > receiver->limit - receiver->used) {
        http_receive_fail(receiver, HTTP_ERROR_LIMIT); return NULL;
    }
    size_t required = receiver->used + size;
    if (required > receiver->capacity) {
        size_t capacity = receiver->capacity ? receiver->capacity :
            (receiver->limit < 65536 ? receiver->limit : 65536);
        while (capacity < required)
            capacity = capacity > receiver->limit / 2 ? receiver->limit : capacity * 2;
        unsigned char *data = realloc(receiver->data, capacity);
        if (!data) { http_receive_fail(receiver, HTTP_ERROR_MEMORY); return NULL; }
        receiver->data = data;
        receiver->capacity = capacity;
    }
    receiver->reserved = size;
    return receiver->data + receiver->used;
}

/* Account only for bytes actually delivered by the host. */
bool http_receive_commit(http_receive_t *receiver, size_t size)
{
    if (!receiver || receiver->status || receiver->error) return false;
    if (!receiver->reserved || size > receiver->reserved) {
        http_receive_fail(receiver, HTTP_ERROR_ARGUMENT); return false;
    }
    receiver->used += size;
    receiver->reserved = 0;
    return true;
}

/* Publish only a completely received final response. */
bool http_receive_finish(http_receive_t *receiver, unsigned status)
{
    if (!receiver || receiver->status || receiver->error) return false;
    if (receiver->reserved || status < 200 || status > 599) {
        http_receive_fail(receiver, HTTP_ERROR_PROTOCOL); return false;
    }
    receiver->status = status;
    return true;
}

/* Borrow completed bytes without making another response copy. */
const unsigned char *http_receive_data(const http_receive_t *receiver)
{
    return receiver && receiver->status ? receiver->data : NULL;
}

/* Report only complete response lengths. */
size_t http_receive_size(const http_receive_t *receiver)
{
    return receiver && receiver->status ? receiver->used : 0;
}

/* Separate HTTP status from transfer failures. */
unsigned http_receive_status(const http_receive_t *receiver)
{
    return receiver ? receiver->status : 0;
}

/* Preserve the first transport failure. */
http_error_t http_receive_error(const http_receive_t *receiver)
{
    return receiver ? receiver->error : HTTP_ERROR_ARGUMENT;
}
