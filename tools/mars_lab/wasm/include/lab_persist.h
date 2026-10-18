/**
 * @file lab_persist.h
 * @brief Application API: worksheet save schemas and deferred-save ownership in C/WebAssembly.
 *
 * Directly indexed schemas define server fields, local storage keys, value
 * sources and empty-value policy for the five mathematical worksheets. Independent
 * save tokens prevent superseded timers from publishing stale snapshots. The host
 * captures DOM values and supplies clock readings, browser storage, timers and
 * Protobuf transport. Mathematical text remains opaque and native MARS alone
 * supplies its canonical representation. No allocation or browser handles are used.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean arguments crossing the WebAssembly i32 ABI must be canonical 0 or 1;
 * C callers use false or true.
 */
#ifndef LAB_WASM_PERSIST_H
#define LAB_WASM_PERSIST_H

#include <stdbool.h>
#include <stdint.h>

#include "lab_mode.h"

/** @brief Storage origin selects schema names and restoration policy; values retain the browser ABI. */
typedef enum {
    LAB_PERSIST_SERVER = 0, /**< Server fields, defaults and trimmed editor text. */
    LAB_PERSIST_LOCAL = 1   /**< Browser storage keys and preserved authored whitespace. */
} lab_persist_origin_t;

/** @brief Captured host value used by a persistence schema field. */
typedef enum {
    LAB_PERSIST_SOURCE_INVALID = -1, /**< Invalid mode or schema field. */
    LAB_PERSIST_TEXT = 0,            /**< Prepared editor text. */
    LAB_PERSIST_TIMESTAMP = 1,       /**< Modification timestamp. */
    LAB_PERSIST_OPERATION = 2,       /**< Matrix operation. */
    LAB_PERSIST_OPERAND = 3,         /**< Matrix operand. */
    LAB_PERSIST_BOUNDS = 4,          /**< Integrator bounds. */
    LAB_PERSIST_INTERVAL_CAP = 5     /**< Integrator interval budget. */
} lab_persist_source_t;

/**
 * @brief Count saved fields for a mathematical worksheet.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4.
 * @return Field count, or zero for invalid/calendar modes.
 */
unsigned lab_persist_count(lab_mode_t mode);

/**
 * @brief Borrow a saved field's immutable UTF-8 name.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based field index below lab_persist_count(mode).
 * @param origin Server field name or browser storage key; other values return an empty string.
 * @return Module-lifetime string; empty for invalid arguments.
 */
const char *lab_persist_text(lab_mode_t mode, unsigned field, lab_persist_origin_t origin);

/**
 * @brief Measure a schema label for the browser adapter.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based field index below lab_persist_count(mode).
 * @param origin Server field name or browser storage key; other values return zero.
 * @return Byte count excluding NUL; zero for invalid arguments.
 */
unsigned lab_persist_text_length(lab_mode_t mode, unsigned field, lab_persist_origin_t origin);

/**
 * @brief Identify the captured value required by a schema field.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based field index below lab_persist_count(mode).
 * @return Text 0, timestamp 1, matrix operation 2, operand 3, bounds 4, interval cap 5; -1 if invalid.
 */
lab_persist_source_t lab_persist_source(lab_mode_t mode, unsigned field);

/**
 * @brief Decide whether a local field may be written.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based field index below lab_persist_count(mode).
 * @param present True if the captured value is non-empty.
 * @return True to write, false to retain the old local value or for invalid indices.
 */
bool lab_persist_local(lab_mode_t mode, unsigned field, bool present);

/**
 * @brief Request the canonical text already supplied by native editor metadata.
 * @param mode Mathematical mode index, 0..4.
 * @return True for equation/integrator; false otherwise. Does not rewrite mathematics.
 */
bool lab_persist_canonical(lab_mode_t mode);

/**
 * @brief Plan restoration of one saved editor without interpreting its mathematics.
 * @param mode Mathematical mode index, 0..4.
 * @param present True if the saved text is non-empty.
 * @param abbreviated True if the host reports the saved text contains an ellipsis marker.
 * @param origin Server or browser storage restoration policy. For ABI compatibility, any non-zero value uses local policy.
 * @return Flags: accept 1, prepare native editor metadata 2, use canonical metadata 4,
 * update expression display/timestamp 8, retain server integrator source 16. Zero skips restoration.
 */
unsigned lab_persist_restore(lab_mode_t mode, bool present, bool abbreviated, lab_persist_origin_t origin);

/**
 * @brief Choose the delay before publishing a snapshot.
 * @param mode Mathematical mode index, 0..4.
 * @param debounce True if the caller requested a deferred save.
 * @return 250 milliseconds for deferred expression/equation saves; otherwise zero.
 */
unsigned lab_persist_delay(lab_mode_t mode, bool debounce);

/**
 * @brief Supersede a mode's pending save without affecting other modes.
 * @param mode Mathematical mode index, 0..4.
 * @param has_text True if the captured editor is non-empty.
 * @return Non-zero token, or zero for invalid mode, blank expression or exhausted serial space.
 * Blank expression leaves pending work intact; blank equation is publishable.
 * Serial exhaustion invalidates all pending tokens and never reuses them.
 */
uint32_t lab_persist_begin(lab_mode_t mode, bool has_text);

/**
 * @brief Claim a snapshot for publication, consuming its token only on success.
 * @param mode Mathematical mode index, 0..4.
 * @param token Token returned by lab_persist_begin.
 * @return True for this mode's current, non-zero token; false for stale/invalid/already consumed tokens.
 */
bool lab_persist_take(lab_mode_t mode, uint32_t token);

#endif
