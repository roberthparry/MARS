#ifndef MARS_HTTP_INTERNAL_H
#define MARS_HTTP_INTERNAL_H
#ifndef MARS_HTTP_INTERNAL_ACCESS
#error "http_internal.h is private to the HTTP module."
#endif
#include <curl/curl.h>
#include "http.h"
#include "array.h"
#if LIBCURL_VERSION_NUM < 0x075500
#error "The HTTP module requires libcurl 7.85.0 or newer."
#endif
struct _http_client_t {
    CURL *easy;
    http_limits_t limits;
    array_t *ca;
    http_cancel_fn cancel;
    void *cancel_data;
    http_error_t error;
    string_t *message;
    bool active;
};
struct _http_request_t {
    webmethod_t method;
    string_t *url;
    dictionary_t *headers;
    unsigned char *body;
    size_t body_size;
    string_t *body_path;
    bool has_body;
};
struct _http_response_t {
    long status;
    dictionary_t *headers;
    array_t *body;
    size_t received;
    bool streamed;
};
dictionary_t *http_headers_new(void);
void http_headers_free(dictionary_t *headers);
bool http_headers_store(dictionary_t *headers, const string_t *name, const string_t *value, bool replace);
const array_t *http_headers_get(const dictionary_t *headers, const string_t *name);
string_t *http_header_name(const string_t *name);
bool http_header_value(const string_t *value);
bool http_ascii_text(const string_t *text, bool spaces);
void http_fail(http_client_t *client, http_error_t code, const char *message);
bool http_global_acquire(void);
void http_global_release(void);
#endif
