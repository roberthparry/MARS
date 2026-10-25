/**
 * @file lab_server_internal.h
 * @brief Private request-authorisation boundary within the Lab server module.
 *
 * Consumed only by server implementation units. Application code uses the opaque
 * lab_server_t interface in lab_server.h and cannot mutate transport routes.
 */
#ifndef MARS_LAB_SERVER_INTERNAL_H
#define MARS_LAB_SERVER_INTERNAL_H

#if !defined(MARS_LAB_SERVER_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_SERVER_INTERNAL_ACCESS."
#endif
#include "webserver-t.h"

/**
 * @brief Check socket identity and browser request headers.
 * @param request Borrowed, parsed request with a socket peer address.
 * @return True only for a permitted private peer, Host and optional Origin.
 */
bool lab_svr_request_permitted(const websrv_request_t *request);

/** @brief Add or replace a response header using bounded literal inputs. */
bool lab_svr_header(websrv_response_t *response, const char *name, const char *value);

/** @brief Borrow a named request header until the request is released. */
const string_t *lab_svr_request_header(const websrv_request_t *request, const char *name);

/** @brief Emit an existing binary error envelope with the supplied HTTP status. */
bool lab_svr_message(const websrv_request_t *request, websrv_response_t *response, unsigned status, const char *text);

/** @brief Opaque image route ownership; its database session is created only when a worker serves an image. */
typedef struct _lab_texture_routes_t lab_texture_routes_t;

/** @brief Register the fixed image address space; return owned route contexts or NULL on failure. */
lab_texture_routes_t *lab_svr_textures_new(websrv_t *listener);

/** @brief Close the process-local image reader and release route contexts; accepts NULL. */
void lab_svr_textures_free(lab_texture_routes_t *routes);
#endif
