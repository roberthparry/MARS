/**
 * @file lab_binding_rows.h
 * @brief Application API: structured integrator row policy for the MARS Lab browser.
 *
 * Owns row defaults, exact-name allocation, native reference retention and
 * reconciliation with result bounds. Existing lab_rows planners enforce edits
 * and the 256-row limit. Authored expressions remain opaque; only native metadata
 * determines references. Browser values and temporary handles are scoped to a
 * synchronous call, with indexed name lookup and bounded temporary storage.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean arguments cross the WebAssembly i32 boundary as canonical 0 or 1, never scoped handles.
 */
#ifndef LAB_WASM_BINDING_ROWS_H
#define LAB_WASM_BINDING_ROWS_H

#include <stdbool.h>

#include "lab_handle.h"
#include "lab_rows.h"

/**
 * @brief Publish the preferred integrator-name catalogue through lab_dom_return.
 * @post Publishes an independent array of nine preferred names followed by x1 through x99.
 */
void lab_binding_row_names(void);

/**
 * @brief Normalise structured row fields without interpreting authored bounds.
 * @param row Scoped record containing kind, name, lo and hi; only kind "free" selects a free row,
 * while falsey bound values become empty strings.
 * @param fallback Scoped fallback name used when the authored name is falsey.
 * @post Publishes the independent normalised record through lab_dom_return.
 */
void lab_binding_row_normalise(browser_handle row, browser_handle fallback);

/**
 * @brief Serialise a structured row array in one bounded call.
 * @param rows Scoped array of at most 256 rows; non-arrays produce empty text.
 * @details Copies opaque name and bound fields verbatim, applying row defaults and fixed notation.
 * Publishes UTF-8 text through lab_dom_return, or null for invalid Unicode or a 4 MiB input/output overflow.
 * No partial text is published. Workspace transfer buffers are temporary scratch storage.
 */
void lab_binding_rows_text(browser_handle rows);

/**
 * @brief Publish an independent single-bound default row array through lab_dom_return.
 * @param blank True selects an x bound with empty limits; false selects x from zero to one.
 */
void lab_binding_rows_default(bool blank);

/**
 * @brief Choose the first unused preferred name using indexed exact-name occupancy.
 * @param rows Scoped authored row array; trimmed names determine occupancy.
 * @post Publishes the selected name through lab_dom_return; exhaustion falls back to x.
 */
void lab_binding_rows_name(browser_handle rows);

/**
 * @brief Normalise render rows using their original authored prefixes for fallback names.
 * @param rows Scoped authored row array; absent, empty or non-array input selects the standard single-bound default.
 * @post Publishes the independent row array through lab_dom_return.
 */
void lab_binding_rows_prepare(browser_handle rows);

/**
 * @brief Capture structured row controls without parsing their mathematical text.
 * @param root Scoped row container; zero or an empty container selects the standard default.
 * @post Publishes the captured row array through lab_dom_return.
 */
void lab_binding_rows_read(browser_handle root);

/**
 * @brief Index native reference metadata by exact name and its source identity.
 * @param expression Scoped opaque source text associated with the native response.
 * @param response Scoped native forms response containing rows and references_valid.
 * @post Publishes the metadata record through lab_dom_return.
 */
void lab_binding_rows_metadata(browser_handle expression, browser_handle response);

/**
 * @brief Retain unknown references conservatively when native metadata is missing or stale.
 * @param metadata Scoped reference metadata returned by lab_binding_rows_metadata.
 * @param expression Scoped current opaque source text.
 * @param name Scoped exact row name.
 * @return True for a referenced row or missing, invalid, stale or unknown metadata; false for an unreferenced row.
 */
bool lab_binding_rows_reference(browser_handle metadata, browser_handle expression, browser_handle name);

/**
 * @brief Resolve the native active-row plan to opaque records and its fallback bound.
 * @param rows Scoped structured row array, limited to 256 entries.
 * @param metadata Scoped native reference metadata, or zero when unavailable.
 * @param expression Scoped current source text used to reject stale metadata.
 * @post Publishes a bounds/rows record through lab_dom_return, or null when oversized.
 */
void lab_binding_rows_plan(browser_handle rows, browser_handle metadata, browser_handle expression);

/**
 * @brief Apply a native row edit atomically, preserving unrelated row metadata.
 * @param rows Scoped structured row array, limited to 256 entries.
 * @param index Zero-based index of the row receiving the edit.
 * @param operation LAB_ROW_TOGGLE (1), LAB_ROW_INSERT (2), or LAB_ROW_REMOVE (3).
 * @post Publishes the edited array through lab_dom_return, or null when the edit is forbidden.
 */
void lab_binding_rows_edit(browser_handle rows, unsigned index, lab_row_operation_t operation);

/**
 * @brief Merge native result bounds with free rows retained by native variable bindings.
 * @param rows Scoped current row array, limited to 256 entries.
 * @param response Scoped native result containing bounds and binding_values.
 * @post Publishes an array through lab_dom_return, null for no supplied bounds, or false on overflow.
 */
void lab_binding_rows_merge(browser_handle rows, browser_handle response);

#endif
