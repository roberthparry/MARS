/**
 * @file websrv_internal.h
 * @brief Private web listener, route and message state.
 *
 * Defines socket ownership, method-indexed route handlers and request or response storage, plus parsing and
 * deadline-I/O helpers. These implementation details remain behind webserver.h's opaque handles.
 *
 * This header is an implementation detail under src/, not an installed public API. Keep its consumers within the
 * documented module boundary and preserve any explicit internal-access guards.
 */

#ifndef MARS_WEBSRV_INTERNAL_H
#define MARS_WEBSRV_INTERNAL_H
#include "webserver.h"
#include <stdint.h>

typedef struct {
    websrv_handler_fn handler[HTTP_OPTIONS + 1];
    void *context[HTTP_OPTIONS + 1];
} websrv_routes_t;

struct _websrv_t {
    int fd;
    uint16_t port;
    websrv_limits_t limits;
    dictionary_t *routes;
    bool active;
};
struct _websrv_request_t {
    webmethod_t method;
    string_t *target, *path;
    unsigned char *body;
    size_t body_size;
    dictionary_t *headers;
};
struct _websrv_response_t {
    unsigned status;
    unsigned char *body;
    size_t body_size;
    dictionary_t *headers;
    size_t max_body, max_headers, header_bytes;
};

dictionary_t *websrv_dict_new(size_t value_size);
void websrv_headers_free(dictionary_t *headers);
string_t *websrv_field_name(const string_t *name);
bool websrv_field_value(const string_t *value);
bool websrv_target_valid(const string_t *target, bool route);
const string_t *websrv_header_get(const dictionary_t *headers, const char *name);
bool websrv_header_put(dictionary_t *headers, string_t *key, string_t *value);
int64_t websrv_now(void);
int websrv_wait(int fd, short events, int64_t deadline);
bool websrv_send(int fd, const void *data, size_t size, int64_t deadline);
int websrv_read_request(int fd, const websrv_limits_t *limits, int64_t deadline, websrv_request_t *request);
bool websrv_write_response(int fd, const websrv_response_t *response, bool head, int64_t deadline);
void websrv_request_clear(websrv_request_t *request);
void websrv_response_clear(websrv_response_t *response);
#endif
