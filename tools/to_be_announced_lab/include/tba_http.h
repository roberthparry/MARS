/**
 * @file tba_http.h
 * @brief Private-network access policy and HTTP header access.
 *
 * Wraps header ownership and private peer, Host and Origin checks. File uploads
 * are disabled; this boundary deliberately has no multipart decoder.
 */
#ifndef TBA_HTTP_H
#define TBA_HTTP_H

#include "webserver-t.h"

/** @brief Check the real socket peer, ignoring untrusted forwarding headers.
 * @param request Borrowed request.
 * @return True only for IPv4 or IPv6 loopback peers. */
bool tba_http_loopback(const websrv_request_t *request);
#include "tba_support.h"

/** @brief Borrow a request header. @param request Request. @param name Header name. @return Borrowed value or NULL. */
const string_t *tba_http_header(const websrv_request_t *request, const char *name);

/** @brief Authorise private peer/Host/Origin. @param request Request. @return Whether access is permitted. */
bool tba_http_permitted(const websrv_request_t *request);

/** @brief Copy a response header. @param response Response. @param name Header. @param value Value. @return Success. */
bool tba_http_header_set(websrv_response_t *response, const char *name, const char *value);

#endif
