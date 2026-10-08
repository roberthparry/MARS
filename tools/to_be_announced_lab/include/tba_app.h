/**
 * @file tba_app.h
 * @brief Opaque application configuration and private persistent storage.
 *
 * Owns defaults and resolved paths for a forecasting server. State updates are
 * locked and atomic; uploads and result files are private. Existing legacy state
 * keys and repository-relative CSV paths are retained. Objects are process-local.
 */
#ifndef TBA_APP_H
#define TBA_APP_H
#include "tba_support.h"
typedef struct tba_app tba_app_t;
/** @brief Configure a Lab. @param binary Optional worker override. @param base Optional URL prefix. @return Owned app or NULL. */
tba_app_t *tba_app_new(const char *binary, const char *base);
/** @brief Release configuration. @param app Owned app; NULL is safe. */
void tba_app_free(tba_app_t *app);
/** @brief Borrow the URL prefix. @param app App. @return Stable string. */
const char *tba_app_base(const tba_app_t *app);
/** @brief Borrow the worker path. @param app App. @return Stable string. */
const char *tba_app_binary(const tba_app_t *app);
/** @brief Load defaults overlaid by saved state. @param app App. @return Owned object or NULL. */
json_t *tba_app_state(const tba_app_t *app);
/** @brief Atomically merge recognised settings. @param app App. @param update Borrowed object. @return Success. */
bool tba_app_save(const tba_app_t *app, const json_t *update);
/** @brief Store a unique private CSV. @param app App. @param text Borrowed UTF-8 contents. @return Owned absolute path or NULL. */
string_t *tba_app_upload(const tba_app_t *app, const string_t *text);
/** @brief Store a successful result. @param app App. @param result Borrowed result. @return Success. */
bool tba_app_result_save(const tba_app_t *app, const json_t *result);
/** @brief Read the most recent result. @param app App. @return Owned object or NULL. */
json_t *tba_app_result(const tba_app_t *app);
#endif
