/**
 * @file lab_persist.h
 * @brief Private interface: worksheet save schemas and deferred-save ownership in C/WebAssembly.
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
 */
#ifndef LAB_WASM_PERSIST_H
#define LAB_WASM_PERSIST_H

#include <stdint.h>

/**
 * @brief Count saved fields for a mathematical worksheet.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4.
 * @return Field count, or zero for invalid/calendar modes.
 */
unsigned lab_persist_count(unsigned mode);

/**
 * @brief Borrow a saved field's immutable UTF-8 name.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based schema field index.
 * @param local Zero selects server field, one selects local storage key.
 * @return Module-lifetime string; empty for invalid arguments.
 */
const char *lab_persist_text(unsigned mode, unsigned field, unsigned local);

/**
 * @brief Measure a schema label for the browser adapter.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based schema field index.
 * @param local Zero selects server field, one selects local storage key.
 * @return Byte count excluding NUL; zero for invalid arguments.
 */
unsigned lab_persist_text_length(unsigned mode, unsigned field, unsigned local);

/**
 * @brief Identify the captured value required by a schema field.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based schema field index.
 * @return Text 0, timestamp 1, matrix operation 2, operand 3, bounds 4, interval cap 5; -1 if invalid.
 */
int lab_persist_source(unsigned mode, unsigned field);

/**
 * @brief Decide whether a local field may be written.
 * @param mode Mathematical mode index, 0..4.
 * @param field Zero-based schema field index.
 * @param present Whether the captured value is non-empty.
 * @return One to write, zero to retain the old local value or for invalid indices.
 */
int lab_persist_local(unsigned mode, unsigned field, int present);

/**
 * @brief Request the canonical text already supplied by native editor metadata.
 * @param mode Mathematical mode index, 0..4.
 * @return One for equation/integrator; zero otherwise. Does not rewrite mathematics.
 */
int lab_persist_canonical(unsigned mode);

/**
 * @brief Plan restoration of one saved editor without interpreting its mathematics.
 * @param mode Mathematical mode index, 0..4.
 * @param present Whether the saved text is non-empty.
 * @param abbreviated Whether the host reports the saved text contains an ellipsis marker.
 * @param local Whether restoring browser storage rather than server state.
 * @return Flags: accept 1, prepare native editor metadata 2, use canonical metadata 4,
 * update expression display/timestamp 8, retain server integrator source 16. Zero skips restoration.
 */
unsigned lab_persist_restore(unsigned mode, int present, int abbreviated, int local);

/**
 * @brief Choose the delay before publishing a snapshot.
 * @param mode Mathematical mode index, 0..4.
 * @param debounce Whether the caller requested a deferred save.
 * @return 250 milliseconds for deferred expression/equation saves; otherwise zero.
 */
unsigned lab_persist_delay(unsigned mode, int debounce);

/**
 * @brief Supersede a mode's pending save without affecting other modes.
 * @param mode Mathematical mode index, 0..4.
 * @param has_text Whether the captured editor is non-empty.
 * @return Non-zero token, or zero for invalid mode, blank expression or exhausted serial space.
 * Blank expression leaves pending work intact; blank equation is publishable.
 * Serial exhaustion invalidates all pending tokens and never reuses them.
 */
uint32_t lab_persist_begin(unsigned mode, int has_text);

/**
 * @brief Claim a snapshot for publication, consuming its token only on success.
 * @param mode Mathematical mode index, 0..4.
 * @param token Token returned by lab_persist_begin.
 * @return One for this mode's current, non-zero token; zero for stale/invalid/already consumed tokens.
 */
int lab_persist_take(unsigned mode, uint32_t token);

#endif
