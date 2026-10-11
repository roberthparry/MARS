/**
 * @file tba_app.h
 * @brief Opaque, process-local private Lab configuration.
 *
 * Owns compiled defaults and a public URL prefix. Client data and settings are
 * never persisted or restored. The json_t type is an in-memory value tree only.
 */
#ifndef TBA_APP_H
#define TBA_APP_H
#include "tba_support.h"
typedef struct tba_app tba_app_t;

/**
 * @brief Create the compiled worksheet defaults without file I/O.
 * @return Independent caller-owned value tree, or NULL on allocation failure; release with json_free.
 */
json_t *tba_app_defaults(void);

/** @brief Configure a Lab. @param base Optional URL prefix. @return Owned app or NULL. */
tba_app_t *tba_app_new(const char *base);
/** @brief Release configuration. @param app Owned app; NULL is safe. */
void tba_app_free(tba_app_t *app);
/** @brief Borrow the URL prefix. @param app App. @return Stable string. */
const char *tba_app_base(const tba_app_t *app);
/** @brief Copy compiled defaults. @param app App. @return Owned object or NULL. */
json_t *tba_app_state(const tba_app_t *app);
#endif
