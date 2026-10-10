/**
 * @file lab_binding.h
 * @brief Private interface: binding and integrator control presentation for the MARS Lab browser.
 *
 * Builds accessible controls from native metadata, orders constants, selects
 * opaque authored values and dispatches browser interactions. Structured row
 * policy lives in lab_binding_rows.c; editor selection and goal-start precedence
 * live in lab_binding_editor.c. No mathematical text is parsed or rewritten.
 * JavaScript owns asynchronous transport and clipboard promises; native
 * continuations own their stale-response guards. Browser Unicode primitives preserve
 * the existing string and locale semantics. Handles are scoped to one call;
 * control metadata retains actual browser values, never integer handles.
 * Rendering and metadata copying visit every entry with bounded temporary
 * storage. Constant ordering uses a stable merge sort rather than repeated scans.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_BINDING_H
#define LAB_WASM_BINDING_H

/** @brief Preserve numeric, case-insensitive browser ordering with its original tie-break.
 * @param arguments Scoped two-item array of binding records or names to compare.
 * @return Negative, zero or positive according to name collation, with ordinary collation breaking numeric ties.
 */
int lab_binding_compare(int arguments);

/** @brief Remove only the identifier's outer display brackets, retaining its exact identity elsewhere.
 * @param arguments Scoped array whose first item is the identifier to display.
 */
void lab_binding_label(int arguments);

/** @brief Recognise UI unset sentinels without interpreting mathematical expressions.
 * @param arguments Scoped array whose first item is the candidate binding value.
 * @return One for blank text, a question mark or case-insensitive NaN after trimming; zero otherwise.
 */
int lab_binding_unset(int arguments);

/** @brief Select the complete authored value; abbreviations are not reconstructed in the browser.
 * @param binding Scoped binding record; value takes precedence over display.
 */
void lab_binding_value(int binding);

/** @brief Only an empty input becomes a question mark; exact symbolic input remains opaque.
 * @param input Scoped binding-value input node; its trimmed empty value becomes a question mark.
 */
void lab_binding_normalised(int input);

/** @brief Capture visible values with the same trimmed names and exact authored expressions.
 * @param root Scoped binding-card container node, or zero to query the document.
 */
void lab_binding_visible(int root);

/** @brief Overlay native-authored then visible values, preserving discovered metadata and unmatched object identity.
 * @param bindings Scoped array of discovered binding records to retain in order.
 * @param authored Scoped array of native-authored values indexed by exact binding name.
 * @param visible Scoped array of visible values; these override authored values for the same name.
 */
void lab_binding_authored(int bindings, int authored, int visible);

/** @brief Build the authored-value overlay within a caller's existing synchronous scope.
 * @param bindings Scoped discovered records, preserving order and unmatched identities.
 * @param authored Scoped native-authored records indexed by exact binding name.
 * @param visible Scoped visible records which take precedence over authored values.
 * @return Scoped independent array, using the same policy as lab_binding_authored.
 */
int lab_binding_authored_values(int bindings, int authored, int visible);

/** @brief Snapshot input identity and untrimmed text for the asynchronous commit ownership checks.
 * @param inputs Scoped list of binding-value input nodes.
 */
void lab_binding_snapshot(int inputs);

/** @brief Recheck exact input bytes after an await; connectivity is deliberately not an additional commit condition.
 * @param snapshot Scoped array returned by lab_binding_snapshot before asynchronous work.
 * @return One if every input still has its captured text, including an empty snapshot; zero otherwise.
 */
int lab_binding_snapshot_current(int snapshot);

/** @brief Apply an accepted commit and return cache additions/deletions for the shared host state.
 * @param snapshot Scoped binding snapshot whose asynchronous commit has already been accepted.
 */
void lab_binding_committed(int snapshot);

/**
 * @brief Project accepted controls and update the authored binding cache.
 * @param snapshot Scoped binding snapshot whose asynchronous commit has already been accepted.
 * @param editor Scoped editor node whose bindingRefreshValid flag is set to true.
 * @param cache Scoped browser Map indexed by exact native binding names.
 * @details Normalises all input values and titles before applying cache additions
 * and unset-value deletions in snapshot order. No mathematical values are evaluated.
 */
void lab_binding_committed_cache(int snapshot, int editor, int cache);

/** @brief Select the requested kind without changing the native binding identity.
 * @param binding Scoped binding record containing the exact name and current kind.
 */
void lab_binding_toggle(int binding);

/** @brief Clear controls without discarding the separate authored-value cache.
 * @param root Scoped binding-card container node to clear and hide.
 */
void lab_binding_clear(int root);

/** @brief Build variable cards in discovery order, followed by stably collated constants.
 * @param root Scoped destination binding-card container node.
 * @param bindings Scoped array of native binding records with names, kinds and authored values.
 */
void lab_binding_render(int root, int bindings);

/** @brief Decide keyboard and click policy; the returned action only schedules host promise work.
 * @param root Scoped binding-card container owning the event target.
 * @param event Scoped browser event with type, target and keyboard/modifier properties.
 */
void lab_binding_event(int root, int event);

/** @brief Restore focus by exact native name and kind after a commit may have replaced all controls.
 * @param root Scoped current binding-card container with its native name/kind index.
 * @param card Scoped previously focused card record containing name and kind.
 * @param editor Scoped fallback editor node when the binding input no longer exists.
 */
void lab_binding_refocus(int root, int card, int editor);

/** @brief Construct integrator rows from sanitised native fields; row-removal policy remains in lab_forms.
 * @param root Scoped destination integrator-row container node.
 * @param rows Scoped array of sanitised rows containing kind, name, lo and hi.
 */
void lab_binding_integrator_render(int root, int rows);

/** @brief Route integrator keyboard, invalidation and row-edit actions through native policy.
 * @param root Scoped integrator-row container owning the event target.
 * @param event Scoped browser event with type, target and keyboard/modifier properties.
 */
void lab_binding_integrator_event(int root, int event);

/** @brief Update inactive prepared fields in place so a blur response cannot discard the next field's focus.
 * @param root Scoped existing integrator-row container node.
 * @param prepared Scoped array of prepared name, lo and hi values aligned with the existing rows.
 */
void lab_binding_integrator_update(int root, int prepared);

/** @brief Compare a scoped binding value with exact text for the binding policy modules.
 * @param value Scoped value to compare without mathematical interpretation.
 * @param text Null-terminated literal string.
 * @return One for an exact match; zero otherwise.
 */
int lab_binding_is(int value, const char *text);

/** @brief Share the binding modules' existing truthy-value fallback.
 * @param value Scoped candidate value.
 * @param fallback Null-terminated fallback text.
 * @return The candidate handle when truthy, otherwise a scoped fallback string handle.
 */
int lab_binding_or(int value, const char *fallback);

/** @brief Obtain a trimmed opaque binding name for presentation and reconciliation.
 * @param binding Scoped binding or integrator-row record.
 * @return Scoped trimmed name, or a scoped empty string when absent.
 */
int lab_binding_name(int binding);

/** @brief Shallow-copy own metadata fields using safe data keys.
 * @param source Scoped source record, or zero for an empty record.
 * @return Scoped independent record retaining the original field values.
 */
int lab_binding_copy(int source);

/** @brief Count only structured arrays supplied to binding policy modules.
 * @param array Scoped candidate array.
 * @return Array entry count, or zero for other value types.
 */
unsigned lab_binding_array_count(int array);

#endif
