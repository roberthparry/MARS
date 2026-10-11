/**
 * @file tba_http_headers.c
 * @brief Borrowed request headers and copied response headers for the Lab.
 *
 * Uses MARS strings at the webserver boundary. No upload decoding is provided;
 * the Lab rejects file-upload routes without parsing or persisting their bodies.
 */
#include "tba_http.h"

/* Borrow a request header through a transient key. */
const string_t *tba_http_header(const websrv_request_t *request, const char *name)
{
    string_t *key = string_new_with(name);
    const string_t *value = key ? websrv_request_header(request, key) : NULL;
    string_free(key);
    return value;
}

/* Copy a response header and release transient string arguments. */
bool tba_http_header_set(websrv_response_t *response, const char *name, const char *value)
{
    string_t *key = string_new_with(name), *text = string_new_with(value);
    bool ok = key && text && websrv_response_header(response, key, text);
    string_free(key);
    string_free(text);
    return ok;
}
