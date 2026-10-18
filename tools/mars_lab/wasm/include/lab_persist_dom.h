/**
 * @file lab_persist_dom.h
 * @brief Application API: persistence and history record assembly for the WASM worksheet.
 *
 * Builds local/server save records and captures mode-specific history fields
 * using the native schemas. Browser storage, timers and asynchronous restoration
 * remain host capabilities. Mathematical editor text arrives already prepared
 * by native MARS; this module does not interpret it or retain borrowed handles.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_PERSIST_DOM_H
#define LAB_WASM_PERSIST_DOM_H

#include "lab_handle.h"
#include "lab_mode.h"

/**
 * @brief Publish the native save schema as a browser array through lab_dom_return.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3 or integrator 4.
 * @details Entries contain field, server, local and source; unsupported modes publish an empty array.
 */
void lab_persist_schema(lab_mode_t mode);

/**
 * @brief Publish local and server save records through lab_dom_return without performing storage I/O.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3 or integrator 4.
 * @param sources Scoped browser array handle indexed by source kind: text 0, Unix-millisecond timestamp 1,
 * matrix operation 2, matrix operand 3, integrator bounds 4 and interval cap 5.
 * @param precision Scoped browser handle to the precision-in-bits value copied to server.precision_bits.
 * @details Publishes an object with local and server records; omitted local values preserve previous storage.
 */
void lab_persist_records(lab_mode_t mode, browser_handle sources, browser_handle precision);

/**
 * @brief Publish a mode's history snapshot from current controls through lab_dom_return.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param text Scoped browser handle to prepared editor text or a calendar summary; surrounding whitespace is removed.
 * @param calendar Scoped calendar-state record handle for modes 5 and 6; zero for other modes.
 * @param bounds Scoped integrator-bounds text handle for mode 4; ignored in other modes.
 * @param cap Scoped validated integrator interval-cap text handle for mode 4; ignored in other modes.
 * @param config Scoped bootstrap-defaults record handle supplying equation-variable and calendar-summary fallbacks.
 * @details Publishes null for an unsupported mode; the host owns the resulting snapshot across calls.
 */
void lab_persist_history(lab_mode_t mode, browser_handle text, browser_handle calendar, browser_handle bounds,
                         browser_handle cap, browser_handle config);

/**
 * @brief Publish a calendar's server save record through lab_dom_return.
 * @param mode DateTime 5 or almanac 6; other modes publish an empty record.
 * @param state Scoped captured-calendar record handle, with keys from the corresponding native profile schema.
 * @param precision Scoped browser handle to the precision-in-bits value copied to precision_bits.
 * @details Calendar field names receive the datetime_ or almanac_ prefix; no storage or network write is performed.
 */
void lab_persist_calendar(lab_mode_t mode, browser_handle state, browser_handle precision);

#endif
