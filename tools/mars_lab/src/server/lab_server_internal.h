/**
 * @file lab_server_internal.h
 * @brief Private request-authorisation boundary within the Lab server module.
 *
 * Consumed only by server implementation units. Application code uses the opaque
 * lab_server_t interface in lab_server.h and cannot mutate transport routes.
 */
#ifndef MARS_LAB_SERVER_INTERNAL_H
#define MARS_LAB_SERVER_INTERNAL_H
#include "webserver.h"

/**
 * @brief Check socket identity and browser request headers.
 * @param request Borrowed, parsed request with a socket peer address.
 * @return True only for a permitted private peer, Host and optional Origin.
 */
bool lab_svr_request_permitted(const websrv_request_t *request);
#endif
