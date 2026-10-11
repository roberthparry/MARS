/**
 * @file tba_http.h
 * @brief Private-network access policy and multipart CSV uploads.
 *
 * Parsers accept bounded UTF-8 text only, reject embedded NULs and malformed
 * multipart boundaries, and never trust uploaded filenames as filesystem paths.
 */
#ifndef TBA_HTTP_H
#define TBA_HTTP_H

#include "webserver.h"
#include "tba_support.h"

/** @brief Borrow a request header. @param request Request. @param name Header name. @return Borrowed value or NULL. */
const string_t *tba_http_header(const websrv_request_t *request, const char *name);

/** @brief Authorise private peer/Host/Origin. @param request Request. @return Whether access is permitted. */
bool tba_http_permitted(const websrv_request_t *request);

/** @brief Extract the single CSV file part. @param request Request. @param filename Receives owned display name. @return Owned CSV text or NULL. */
string_t *tba_http_upload(const websrv_request_t *request, string_t **filename);

/** @brief Copy a response header. @param response Response. @param name Header. @param value Value. @return Success. */
bool tba_http_header_set(websrv_response_t *response, const char *name, const char *value);

#endif
