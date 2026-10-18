/**
 * @file lab_binding.h
 * @brief Application API for binding and integrator control presentation in the MARS Lab browser.
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
 * Binding cards and workspace calculus controls share the scoped identifier-label formatter.
 * Shared implementation helpers belong to lab_binding_internal.h; other modules use this API.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_BINDING_H
#define LAB_WASM_BINDING_H

#include <stdbool.h>

#include "lab_handle.h"

/**
 * @brief Preserve numeric, case-insensitive browser ordering with its original tie-break.
 * @param arguments Scoped two-item array of binding records or names to compare.
 * @return Negative, zero or positive according to name collation, with ordinary collation breaking numeric ties.
 */
int lab_binding_compare(browser_handle arguments);

/**
 * @brief Remove only the identifier's outer display brackets, retaining its exact identity elsewhere.
 * @param arguments Scoped array whose first item is the identifier to display.
 * @post Publishes the display string through lab_dom_return.
 */
void lab_binding_label(browser_handle arguments);

/**
 * @brief Format an identifier within the caller's existing synchronous handle scope.
 * @param name Scoped identifier value, using the same truthy fallback and outer-bracket removal as lab_binding_label.
 * @return Scoped display-string handle; the caller releases it and any temporary handles after use.
 * @details Preserves whitespace, Unicode and inner brackets without interpreting mathematical text.
 */
browser_handle lab_binding_label_value(browser_handle name);

/**
 * @brief Recognise UI unset sentinels without interpreting mathematical expressions.
 * @param arguments Scoped array whose first item is the candidate binding value.
 * @return True for blank text, a question mark or case-insensitive NaN after trimming; false otherwise.
 */
bool lab_binding_unset(browser_handle arguments);

/**
 * @brief Select the complete authored value; abbreviations are not reconstructed in the browser.
 * @param binding Scoped binding record; truthy value takes precedence over display.
 * @post Publishes trimmed text through lab_dom_return; blank, question-mark and NaN sentinels become empty text.
 */
void lab_binding_value(browser_handle binding);

/**
 * @brief Only an empty input becomes a question mark; exact symbolic input remains opaque.
 * @param input Scoped binding-value input node; its trimmed empty value becomes a question mark.
 * @post Publishes the normalised text through lab_dom_return without changing the input.
 */
void lab_binding_normalised(browser_handle input);

/**
 * @brief Capture visible values with the same trimmed names and exact authored expressions.
 * @param root Scoped binding-card container node, or zero to query the document.
 * @post Publishes named records with name, kind, value and display through lab_dom_return.
 */
void lab_binding_visible(browser_handle root);

/**
 * @brief Overlay native-authored then visible values, preserving discovered metadata and unmatched object identity.
 * @param bindings Scoped array of discovered binding records to retain in order.
 * @param authored Scoped array of native-authored values indexed by exact binding name.
 * @param visible Scoped array of visible values; these override authored values for the same name.
 * @post Publishes the independent overlay array through lab_dom_return.
 */
void lab_binding_authored(browser_handle bindings, browser_handle authored, browser_handle visible);

/**
 * @brief Build the authored-value overlay within a caller's existing synchronous scope.
 * @param bindings Scoped discovered records, preserving order and unmatched identities.
 * @param authored Scoped native-authored records indexed by exact binding name.
 * @param visible Scoped visible records which take precedence over authored values.
 * @return Scoped independent array, using the same policy as lab_binding_authored.
 */
browser_handle lab_binding_authored_values(browser_handle bindings, browser_handle authored, browser_handle visible);

/**
 * @brief Snapshot input identity and untrimmed text for the asynchronous commit ownership checks.
 * @param inputs Scoped array or NodeList of binding-value input nodes.
 * @post Publishes records containing input identity, untrimmed text and normalised binding fields.
 */
void lab_binding_snapshot(browser_handle inputs);

/**
 * @brief Recheck exact input bytes after an await; connectivity is deliberately not an additional commit condition.
 * @param snapshot Scoped array returned by lab_binding_snapshot before asynchronous work.
 * @return True if every input still has its captured text, including an empty snapshot; false otherwise.
 */
bool lab_binding_snapshot_current(snapshot_handle snapshot);

/**
 * @brief Apply an accepted commit and return cache additions/deletions for the shared host state.
 * @param snapshot Scoped binding snapshot whose asynchronous commit has already been accepted.
 * @post Publishes name/value pairs through lab_dom_return; a null value requests cache deletion.
 */
void lab_binding_committed(snapshot_handle snapshot);

/**
 * @brief Project accepted controls and update the authored binding cache.
 * @param snapshot Scoped binding snapshot whose asynchronous commit has already been accepted.
 * @param editor Scoped editor node whose bindingRefreshValid flag is set to true.
 * @param cache Scoped browser Map indexed by exact native binding names.
 * @details Normalises all input values and titles before applying cache additions
 * and unset-value deletions in snapshot order. No mathematical values are evaluated.
 */
void lab_binding_committed_cache(snapshot_handle snapshot, browser_handle editor, browser_handle cache);

/**
 * @brief Select the requested kind without changing the native binding identity.
 * @param binding Scoped binding record containing name and kind; missing kind defaults to variable.
 * @post Publishes trimmed name and nextKind through lab_dom_return; constant toggles to variable and vice versa.
 */
void lab_binding_toggle(browser_handle binding);

/**
 * @brief Clear controls without discarding the separate authored-value cache.
 * @param root Scoped binding-card container node to clear and hide.
 */
void lab_binding_clear(browser_handle root);

/**
 * @brief Build variable cards in discovery order, followed by stably collated constants.
 * @param root Scoped destination binding-card container node.
 * @param bindings Scoped array of native binding records with names, kinds and authored values.
 * @post Publishes values and kinds arrays of installed controls through lab_dom_return.
 */
void lab_binding_render(browser_handle root, browser_handle bindings);

/**
 * @brief Decide keyboard and click policy; the returned action only schedules host promise work.
 * @param root Scoped binding-card container owning the event target.
 * @param event Scoped browser event with type, target and keyboard/modifier properties.
 * @post Publishes an action/card/prevent record, or null for an unrelated or unhandled event.
 */
void lab_binding_event(browser_handle root, event_handle event);

/**
 * @brief Restore focus by exact native name and kind after a commit may have replaced all controls.
 * @param root Scoped current binding-card container with its native name/kind index.
 * @param card Scoped previously focused card record containing name and kind.
 * @param editor Scoped fallback editor node when the binding input no longer exists.
 */
void lab_binding_refocus(browser_handle root, browser_handle card, browser_handle editor);

/**
 * @brief Construct integrator rows from sanitised native fields; row-removal policy remains in lab_forms.
 * @param root Scoped destination integrator-row container node.
 * @param rows Scoped array of sanitised rows containing kind, name, lo and hi.
 */
void lab_binding_integrator_render(browser_handle root, browser_handle rows);

/**
 * @brief Route integrator keyboard, invalidation and row-edit actions through native policy.
 * @param root Scoped integrator-row container owning the event target.
 * @param event Scoped browser event with type, target and keyboard/modifier properties.
 * @post Publishes an action record or null; row edits include index and operation (toggle 1, insert 2, remove 3).
 */
void lab_binding_integrator_event(browser_handle root, event_handle event);

/**
 * @brief Update inactive prepared fields in place so a blur response cannot discard the next field's focus.
 * @param root Scoped existing integrator-row container node.
 * @param prepared Scoped array of prepared name, lo and hi values aligned with the existing rows.
 */
void lab_binding_integrator_update(browser_handle root, browser_handle prepared);

/**
 * @brief Shallow-copy own metadata fields using safe data keys.
 * @param source Scoped source record, or zero for an empty record.
 * @return Scoped independent record retaining the original field values.
 */
browser_handle lab_binding_copy(browser_handle source);

/**
 * @brief Count only structured arrays supplied to binding policy modules.
 * @param array Scoped candidate array.
 * @return Array entry count, or zero for other value types.
 */
unsigned lab_binding_array_count(browser_handle array);

#endif
