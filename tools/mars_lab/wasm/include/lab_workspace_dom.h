/**
 * @file lab_workspace_dom.h
 * @brief Application API for workspace controls, visibility and editor projection in the C/WASM Lab.
 *
 * Applies mode and precision policy, help selection, result-card visibility,
 * busy-state ownership, conditional editor sizing and calculus action markup.
 * Calculus labels use the shared binding formatter, with temporary handles released per name.
 * Captures and restores opaque DOM snapshots without interpreting mathematical
 * text. Browser values and nodes are borrowed only within a synchronous call;
 * returned snapshot objects belong to the host and no handles are retained.
 * Decoded snapshot validation preserves state identity and uses the supplied mode catalogue.
 * Result restoration normalises opaque metadata, clones variable lists and
 * projects reusable input before returning globals for calculus-control refresh.
 * Request lifetimes and asynchronous browser callbacks remain with their adapters.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean arguments crossing the WebAssembly i32 ABI must be canonical 0 or 1;
 * C callers use false or true.
 */
#ifndef LAB_WASM_WORKSPACE_DOM_H
#define LAB_WASM_WORKSPACE_DOM_H

#include <stdbool.h>

#include "lab_handle.h"
#include "lab_mode.h"

/**
 * @brief Append an editable timeline row using safe worksheet or saved-place labels and native action labels.
 * @param list Borrowed ordered-list DOM handle receiving the row.
 * @param snapshot Borrowed decoded worksheet snapshot. Calendar modes use saved town, jurisdiction and date fields;
 * other modes use their authored mathematical text. Unknown calendar places retain identifying coordinates.
 * @param position Zero-based chronological position stored on the row for event routing.
 * @param current Position of the active worksheet, marked with aria-current.
 * @param count Total entries, used to disable actions at timeline boundaries and removal of the sole entry.
 * @return No value. Browser nodes remain owned by the list; no handles or snapshots are retained.
 * Call inside a synchronous labDOM scope on the owning browser thread. Mathematical text is never evaluated.
 */
void lab_workspace_dom_history_row(jsref list, jsref snapshot, unsigned position, unsigned current, unsigned count);

/**
 * @brief Apply country names and SVG flag artwork to an existing calendar history row.
 * @param button Borrowed history-open button created by lab_workspace_dom_history_row(); unrelated nodes are ignored.
 * @param metadata Borrowed country response or cached name/flag record. Text is copied safely; only supported local
 * SVG artwork is accepted. Unavailable fields and invalid artwork leave their corresponding display unchanged.
 * @return No value. The browser retains copied text and artwork references; no scoped handle is retained.
 * @note Call synchronously on the owning browser thread after checking the asynchronous request is still current.
 * This updates presentation only and never changes worksheet snapshots, the selected observer or history ordering.
 */
void lab_workspace_dom_history_country(node_jsref button, jsref metadata);

/** @brief Requested help/result pane visibility transition. */
typedef enum {
    LAB_WORKSPACE_SHOW_RESULTS = 0, /**< Show results without updating status text. */
    LAB_WORKSPACE_SHOW_HELP = 1,    /**< Show help and update status. */
    LAB_WORKSPACE_TOGGLE_HELP = 2   /**< Toggle help visibility and update status. */
} lab_workspace_help_action_t;

/**
 * @brief Validate a decoded worksheet snapshot and publish its original state through lab_dom_return.
 * @param envelope Scoped handle to a decoded non-null object with a state property; arrays with that property
 * are accepted.
 * @param mode_names Scoped handle to an array of at most seven permitted mode strings, compared exactly.
 * @details Returns the same state object when mode and text are strings and the mode is permitted.
 * Explicit state:null returns null. Returns numeric -1 for an invalid envelope and -2 for invalid state,
 * including state:undefined or a non-array or oversized mode catalogue. Property presence includes
 * inherited properties. Inputs are decoded data, not functions, accessors or other executable values.
 * No input is mutated, no exception is requested, and no handle survives this synchronous call.
 */
void lab_workspace_snapshot_validate(jsref envelope, jsref mode_names);

/**
 * @brief Return named browser control references for startup through lab_dom_return.
 * @details The fixed catalogue resolves exact IDs and snapshots selector collections
 * into arrays. Missing controls become null. The host owns returned browser values;
 * the module retains no handles and does not install event subscriptions here.
 */
void lab_workspace_dom_references(void);

/** @brief Return current precision status text through lab_dom_return.
 */
void lab_workspace_dom_precision(void);

/**
 * @brief Publish the six title/subtitle labels for one worksheet mode.
 * @param mode Worksheet mode index 0..6; invalid indices select Expression.
 * @details lab_dom_return supplies title, subtitle, Rendered, Expression, Function and Value labels in that order.
 */
void lab_workspace_dom_labels(lab_mode_t mode);

/** @brief Project status text with native precision.
 * @param text Scoped status text to precede the current precision.
 */
void lab_workspace_dom_status(jsref text);

/** @brief Project tab selection and the compact Mode menu using an exact mode token.
 * Copies the active tab's label into the menu summary, closes the phone menu and
 * leaves desktop tabs expanded. Focus returns to the phone summary when needed.
 * Absent optional menu nodes are ignored. The call retains no supplied handles.
 * @param tabs Scoped list of registered mode-tab nodes.
 * @param mode Scoped exact mode-name string to match against each tab's dataset.
 */
void lab_workspace_dom_tabs(jsref tabs, jsref mode);

/** @brief Project result titles and Function execution visibility.
 * @param rendered Scoped Rendered card title string.
 * @param parsed Scoped Expression card title string.
 * @param function Scoped Function card title string; exactly "Function" enables its run control.
 * @param value Scoped Value card title string.
 */
void lab_workspace_dom_titles(jsref rendered, jsref parsed, jsref function, jsref value);

/** @brief Set auxiliary visibility and relinquish hidden card expansion.
 * @param visible True shows the controls; false hides them.
 */
void lab_workspace_dom_aux(bool visible);

/** @brief Set Value visibility without overriding expansion CSS.
 * @param visible True shows the controls; false hides them.
 */
void lab_workspace_dom_value(bool visible);

/** @brief Project mode panels and titles.
 * @param mode Worksheet mode index, 0..6.
 * @param coverage Scoped almanac coverage text appended to its subtitle.
 */
void lab_workspace_dom_mode(lab_mode_t mode, jsref coverage);

/** @brief Project matrix operand visibility.
 * @param mode Worksheet mode index, 0..6.
 */
void lab_workspace_dom_matrix(lab_mode_t mode);

/** @brief Project help cards using exact mode membership.
 * @param cards Scoped list of help-card nodes with comma-separated data-help-modes tags.
 * @param mode Scoped exact mode-name string to match after trimming each tag.
 */
void lab_workspace_dom_help_cards(jsref cards, jsref mode);

/** @brief Show results (0), help (1), or toggle (2).
 * @param action Show results 0, show help 1 or toggle 2; other values do nothing.
 */
void lab_workspace_dom_help(lab_workspace_help_action_t action);

/** @brief Project goal entry and queue focus/selection when shown.
 * @param visible True shows the controls; false hides them.
 */
void lab_workspace_dom_target(bool visible);

/** @brief Project worksheet availability while retaining precision edits during asynchronous evaluation.
 * Precision controls remain usable while busy. An active slider preview and an open custom-precision draft
 * survive control refreshes; committing precision uses the ordinary replacement-evaluation workflow.
 * @param mode Worksheet mode index, 0..6.
 * @param busy True disables actions that require a completed result, while replacement actions remain available.
 * @param ready True when the current input can be evaluated.
 * @param back Number of available backward-history entries.
 * @param forward Number of available forward-history entries.
 * @param goal True when variable bindings permit goal seeking.
 * @param minimum True when precision is already at its lower limit.
 * @param maximum True when precision is already at its upper limit.
 * @return Nothing. Updates the current page's controls without retaining input handles or starting calculations.
 */
void lab_workspace_dom_controls(lab_mode_t mode, bool busy, bool ready, unsigned back, unsigned forward, bool goal,
                                bool minimum, bool maximum);

/**
 * @brief Set the precision slider position and accessible readout without changing calculation state.
 * @param bits Requested binary precision; the track shows its nearest offered position within 53..6656 bits,
 * while the readout retains the requested value, including custom values above the track limit.
 */
void lab_workspace_dom_precision_slider(unsigned bits);

/**
 * @brief Update the precision readout without resetting the range input's pending change event.
 * @param bits Binary precision to display in the digit count, tooltip and aria-valuetext; values above 6656
 * receive a Custom label. This does not move the thumb or apply precision to the worksheet.
 */
void lab_workspace_dom_precision_preview(unsigned bits);

/** @brief Project busy state without re-enabling initially disabled controls.
 * @param busy True disables controls; false releases the busy state.
 * @param equation_variable Scoped equation-variable control node.
 * @param copies Scoped list of copy-button nodes.
 * @param digits Scoped list of more-digits button nodes.
 */
void lab_workspace_dom_busy(bool busy, jsref equation_variable, jsref copies, jsref digits);

/** @brief Project a button's running and accessibility state.
 * @param button Scoped action-button node.
 * @param running True applies running styling and aria-busy; false removes both.
 */
void lab_workspace_dom_running(jsref button, bool running);

/** @brief Report connected positive-height editor layout.
 * @param editor Scoped editor node.
 * @return True for a connected editor with visible layout and positive client height; false otherwise.
 */
bool lab_workspace_dom_editor_visible(jsref editor);

/** @brief Clear sizing state on all supplied editors.
 * @param editors Scoped list of editor nodes whose manual sizing is cleared.
 */
void lab_workspace_dom_editor_reset(jsref editors);

/** @brief Measure and resize editors within the viewport budget.
 * @param editors Scoped list of editor nodes sharing the available height budget.
 * @param viewport Viewport height in CSS pixels.
 */
void lab_workspace_dom_editor_resize(jsref editors, double viewport);

/** @brief Build calculus controls from exact names and display labels.
 * @param names Scoped array of exact variable-name strings in discovery order.
 * @param labels Scoped array of display labels aligned with names.
 * @param differentiable True creates derivative and integral buttons; false leaves the panel empty.
 */
void lab_workspace_dom_derivatives(jsref names, jsref labels, bool differentiable);

/** @brief Derive native binding labels and render calculus controls through lab_workspace_dom_derivatives.
 * @param names Scoped array of exact variable-name strings in discovery order; it is not modified.
 * @param differentiable True creates derivative and integral buttons; false leaves the panel empty.
 * @details Visible plot controls receive the same names and labels regardless of differentiability.
 * The shared lab_binding_label_value formatter removes only outer display brackets. Temporary handles
 * are released per name and all handles created here are released before returning; no handle is retained.
 */
void lab_workspace_dom_calculus(jsref names, bool differentiable);

/**
 * @brief Publish an opaque snapshot of a DOM node.
 * @param node Scoped node to capture, including its class name and dataset.
 * @param button True additionally captures textContent and disabled; false captures innerHTML and inline style.
 * @details lab_dom_return supplies the browser-owned snapshot used by lab_workspace_dom_restore.
 */
void lab_workspace_dom_snapshot(node_jsref node, bool button);

/**
 * @brief Restore a node snapshot and remove obsolete dataset fields.
 * @param node Scoped destination node whose class name and complete dataset are restored.
 * @param state Scoped snapshot returned by lab_workspace_dom_snapshot.
 * @param button True restores textContent and disabled; false restores innerHTML and inline style.
 */
void lab_workspace_dom_restore(node_jsref node, jsref state, bool button);

/** @brief Report nonblank content in result cards.
 * @return True when rendered HTML or auxiliary result text is nonblank; false otherwise.
 */
bool lab_workspace_dom_has_result(void);

/** @brief Initialise the fixed per-mode table of absent result snapshots.
 * @post lab_dom_return supplies a new object with all seven mode keys set to null.
 * The host retains this object; native code does not retain its handle.
 */
void lab_workspace_dom_result_states(void);

/** @brief Return a complete snapshot from normalised metadata, or null when blank.
 * @param metadata Scoped record containing lastTex, lastDerivativeExpression, currentVariables and
 * currentDifferentiable. Missing or false-like text becomes empty; only boolean false disables calculus.
 * @post lab_dom_return supplies the snapshot with an independent variable array (empty for
 * non-array inputs), copied DOM datasets and the current reusable-input text, or null for blank cards.
 */
void lab_workspace_dom_result_save(jsref metadata);

/** @brief Restore result cards and reusable input, returning normalised metadata for the host globals.
 * @param state Scoped result snapshot from lab_workspace_dom_result_save, or zero when absent.
 * @post lab_dom_return supplies lastTex, lastDerivativeExpression, currentVariables,
 * currentDifferentiable and an empty resultInputBindings array. Variable arrays are copied on each restore.
 * Missing state returns null for the host's full-clear path. The host must cancel pending Function work
 * before calling, assign the returned globals before refreshing derivative controls, then schedule rendered
 * and solver fitting. The function invalidates old render ownership before projecting any saved DOM.
 */
void lab_workspace_dom_result_restore(jsref state);

#endif
