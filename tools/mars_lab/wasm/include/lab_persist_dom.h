/**
 * @file lab_persist_dom.h
 * @brief Private interface: persistence and history record assembly for the WASM worksheet.
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

/** @brief Return the native save schema as a browser array. @param mode Mathematical mode index 0..4. */
void lab_persist_schema(unsigned mode);

/** @brief Return local and server save records. @param mode Mathematical mode index.
 * @param sources Captured source array in native schema order. @param precision Precision record handle. */
void lab_persist_records(unsigned mode, int sources, int precision);

/** @brief Return a mode's history snapshot from current controls. @param mode Mode index 0..6.
 * @param text Prepared editor or calendar summary. @param calendar Calendar record, or zero.
 * @param bounds Integrator bounds text. @param cap Validated interval cap text. @param config Bootstrap defaults. */
void lab_persist_history(unsigned mode, int text, int calendar, int bounds, int cap, int config);

/** @brief Return a calendar's server save record. @param mode DateTime 5 or Almanac 6.
 * @param state Captured calendar state. @param precision Precision record. */
void lab_persist_calendar(unsigned mode, int state, int precision);

#endif
