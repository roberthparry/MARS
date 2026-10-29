/**
 * @file mars_settings_internal.h
 * @brief Private protocol between settings accessors and their isolated helper.
 *
 * Only the shared settings implementation consumes this interface. The helper owns encrypted
 * database and key-store access; callers receive copied values through a private local socket.
 * Applications use mars_settings.h and must start isolation before opening stores or threads.
 */
#ifndef MARS_SETTINGS_INTERNAL_H
#define MARS_SETTINGS_INTERNAL_H

#if !defined(MARS_SETTINGS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SETTINGS_INTERNAL_ACCESS."
#endif

#include "mars_settings.h"

/** @brief Fixed protocol operations, used as direct indices into the helper's handler table. */
typedef enum {
    MARS_SETTINGS_LOAD,
    MARS_SETTINGS_RESOURCE,
    MARS_SETTINGS_SYNC,
    MARS_SETTINGS_REQUEST_COUNT
} mars_settings_request_t;

/**
 * @brief Report whether this process has opted into isolated settings access.
 * @return True even after a transport failure; callers must never fall back to local decryption.
 */
bool mars_settings_process_enabled(void);

/**
 * @brief Run one serialised request in the owned helper process.
 * @param request Valid protocol operation.
 * @param input Borrowed UTF-8 request payload, or NULL for no payload.
 * @param[out] output Required pointer set to an owned reply or NULL when absent; wipe sensitive bytes and string_free.
 * @return True on successful operation and transport; false on invalid input, helper failure or inherited client state.
 * @note The client mutex serialises threads; a forked child cannot reuse its parent's channel.
 */
bool mars_settings_process_request(mars_settings_request_t request, const string_t *input, string_t **output);

/**
 * @brief Read unresolved application settings as a private configuration snapshot.
 * @return Owned JSON text with literal values, or NULL on invalid configuration or unavailable key; release with
 * string_free after wiping its bytes. Only the settings helper or a non-isolated caller may invoke this function.
 */
string_t *mars_settings_snapshot(void);

#endif
