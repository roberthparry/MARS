/**
 * @file lab_state.h
 * @brief Persistent native Lab worksheets shared by prefork workers.
 *
 * The store honours MARS_HOME and MARS_LAB_STATE_FILE, serialises cooperating
 * processes through a separate advisory lock, and atomically replaces private
 * JSON files. Callers own loaded JSON. All writers must use this interface;
 * the legacy Python writer does not participate in its locking protocol.
 */
#ifndef MARS_LAB_STATE_H
#define MARS_LAB_STATE_H

#include "json.h"

/**
 * @brief Load owned state with defaults, or NULL on I/O, parsing or locking failure.
 * Missing files yield defaults. Malformed files are preserved, never overwritten.
 * @return Owned state object released with json_free, or NULL on failure.
 */
json_t *lab_state_load(void);

/**
 * @brief Merge recognised updates under an interprocess lock and save atomically.
 * Unknown keys and invalid value types are ignored. Precision is merged per mode;
 * stale expression/equation timestamps cannot overwrite newer edits. Returns false
 * on failure. A directory-sync failure may be reported after replacement succeeds.
 * @param updates Borrowed JSON object containing changed worksheet settings.
 * @return True if the update was saved and synchronised; false on failure.
 */
bool lab_state_save(const json_t *updates);

#endif
