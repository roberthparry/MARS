/**
 * @file tba_server.h
 * @brief Opaque native HTTP listener for the forecasting workbench.
 *
 * Registers exact private routes and bounded asset/upload handlers. Construct
 * before forking; each single-threaded worker may serve its inherited listener.
 * The borrowed app must outlive the server. No threads or Python are required.
 */
#ifndef TBA_SERVER_H
#define TBA_SERVER_H
#include <stdint.h>
#include "tba_app.h"
typedef struct tba_server tba_server_t;
/** @brief Bind the Lab. @param app Borrowed configuration. @param host Numeric bind address. @param port Port or zero. @return Owned server or NULL. */
tba_server_t *tba_svr_new(tba_app_t *app, const char *host, uint16_t port);
/** @brief Release an idle listener. @param server Owned server; NULL is safe. */
void tba_svr_free(tba_server_t *server);
/** @brief Borrow the actual port. @param server Server. @return Port or zero. */
uint16_t tba_svr_port(const tba_server_t *server);
/** @brief Serve one request. @param server Server. @return websrv_serve_once result. */
int tba_svr_once(tba_server_t *server);
#endif
