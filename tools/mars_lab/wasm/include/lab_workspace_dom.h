/**
 * @file lab_workspace_dom.h
 * @brief Private interface: workspace controls, visibility and editor projection for the C/WASM Lab.
 *
 * Applies mode and precision policy, help selection, result-card visibility,
 * busy-state ownership, conditional editor sizing and calculus action markup.
 * Captures and restores opaque DOM snapshots without interpreting mathematical
 * text. Browser values and nodes are borrowed only within a synchronous call;
 * returned snapshot objects belong to the host and no handles are retained.
 * Result restoration normalises opaque metadata, clones variable lists and
 * projects reusable input before returning globals for calculus-control refresh.
 * Request lifetimes and asynchronous browser callbacks remain with their adapters.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_WORKSPACE_DOM_H
#define LAB_WASM_WORKSPACE_DOM_H

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

/** @brief Return the six native mode labels through lab_dom_return.
 * @param mode Worksheet mode index, 0..6.
 */
void lab_workspace_dom_labels(unsigned mode);

/** @brief Project status text with native precision.
 * @param text Scoped status text to precede the current precision.
 */
void lab_workspace_dom_status(int text);

/** @brief Project tab selection using an exact mode token.
 * @param tabs Scoped list of registered mode-tab nodes.
 * @param mode Scoped exact mode-name string to match against each tab's dataset.
 */
void lab_workspace_dom_tabs(int tabs, int mode);

/** @brief Project result titles and Function execution visibility.
 * @param rendered Scoped Rendered card title string.
 * @param parsed Scoped Expression card title string.
 * @param function Scoped Function card title string; exactly "Function" enables its run control.
 * @param value Scoped Value card title string.
 */
void lab_workspace_dom_titles(int rendered, int parsed, int function, int value);

/** @brief Set auxiliary visibility and relinquish hidden card expansion.
 * @param visible Non-zero shows the controls; zero hides them.
 */
void lab_workspace_dom_aux(int visible);

/** @brief Set Value visibility without overriding expansion CSS.
 * @param visible Non-zero shows the controls; zero hides them.
 */
void lab_workspace_dom_value(int visible);

/** @brief Project mode panels and titles.
 * @param mode Worksheet mode index, 0..6.
 * @param coverage Scoped almanac coverage text appended to its subtitle.
 */
void lab_workspace_dom_mode(unsigned mode, int coverage);

/** @brief Project matrix operand visibility.
 * @param mode Worksheet mode index, 0..6.
 */
void lab_workspace_dom_matrix(unsigned mode);

/** @brief Project help cards using exact mode membership.
 * @param cards Scoped list of help-card nodes with comma-separated data-help-modes tags.
 * @param mode Scoped exact mode-name string to match after trimming each tag.
 */
void lab_workspace_dom_help_cards(int cards, int mode);

/** @brief Show results (0), help (1), or toggle (2).
 * @param action Show results 0, show help 1 or toggle 2; other values do nothing.
 */
void lab_workspace_dom_help(unsigned action);

/** @brief Project goal entry and queue focus/selection when shown.
 * @param visible Non-zero shows the controls; zero hides them.
 */
void lab_workspace_dom_target(int visible);

/** @brief Project worksheet availability and disabled explanations.
 * @param mode Worksheet mode index, 0..6.
 * @param busy Non-zero disables all worksheet actions while a request is running.
 * @param ready Non-zero when the current input can be evaluated.
 * @param back Number of available backward-history entries.
 * @param forward Number of available forward-history entries.
 * @param goal Non-zero when variable bindings permit goal seeking.
 * @param minimum Non-zero when precision is already at its lower limit.
 * @param maximum Non-zero when precision is already at its upper limit.
 */
void lab_workspace_dom_controls(unsigned mode, int busy, int ready, unsigned back, unsigned forward, int goal,
                                int minimum, int maximum);

/** @brief Project busy state without re-enabling initially disabled controls.
 * @param busy Non-zero disables controls; zero releases the busy state.
 * @param equation_variable Scoped equation-variable control node.
 * @param copies Scoped list of copy-button nodes.
 * @param digits Scoped list of more-digits button nodes.
 */
void lab_workspace_dom_busy(int busy, int equation_variable, int copies, int digits);

/** @brief Project a button's running and accessibility state.
 * @param button Scoped action-button node.
 * @param running Non-zero applies running styling and aria-busy; zero removes both.
 */
void lab_workspace_dom_running(int button, int running);

/** @brief Report connected positive-height editor layout.
 * @param editor Scoped editor node.
 * @return One for a connected editor with visible layout and positive client height; zero otherwise.
 */
int lab_workspace_dom_editor_visible(int editor);

/** @brief Clear sizing state on all supplied editors.
 * @param editors Scoped list of editor nodes whose manual sizing is cleared.
 */
void lab_workspace_dom_editor_reset(int editors);

/** @brief Measure and resize editors within the viewport budget.
 * @param editors Scoped list of editor nodes sharing the available height budget.
 * @param viewport Viewport height in CSS pixels.
 */
void lab_workspace_dom_editor_resize(int editors, double viewport);

/** @brief Build calculus controls from exact names and display labels.
 * @param names Scoped array of exact variable-name strings in discovery order.
 * @param labels Scoped array of display labels aligned with names.
 * @param differentiable Non-zero creates derivative and integral buttons; zero leaves the panel empty.
 */
void lab_workspace_dom_derivatives(int names, int labels, int differentiable);

/** @brief Return a node snapshot; button selects button-specific fields.
 * @param node Scoped node to capture.
 * @param button Non-zero captures text and disabled state; zero captures HTML and inline style.
 */
void lab_workspace_dom_snapshot(int node, int button);

/** @brief Restore a snapshot and remove obsolete dataset fields.
 * @param node Scoped destination node.
 * @param state Scoped snapshot returned by lab_workspace_dom_snapshot.
 * @param button Non-zero restores button fields; zero restores HTML and inline style.
 */
void lab_workspace_dom_restore(int node, int state, int button);

/** @brief Report nonblank content in result cards.
 * @return One when rendered HTML or auxiliary result text is nonblank; zero otherwise.
 */
int lab_workspace_dom_has_result(void);

/** @brief Initialise the fixed per-mode table of absent result snapshots.
 * @return No C value; lab_dom_return supplies a new object with all seven mode keys set to null.
 * The host retains this object; native code does not retain its handle.
 */
void lab_workspace_dom_result_states(void);

/** @brief Return a complete snapshot from normalised metadata, or null when blank.
 * @param metadata Scoped record containing lastTex, lastDerivativeExpression, currentVariables and
 * currentDifferentiable. Missing or false-like text becomes empty; only boolean false disables calculus.
 * @return No C value; lab_dom_return supplies the snapshot with an independent variable array (empty for
 * non-array inputs), copied DOM datasets and the current reusable-input text, or null for blank cards.
 */
void lab_workspace_dom_result_save(int metadata);

/** @brief Restore result cards and reusable input, returning normalised metadata for the host globals.
 * @param state Scoped result snapshot from lab_workspace_dom_result_save, or zero when absent.
 * @return No C value; lab_dom_return supplies lastTex, lastDerivativeExpression, currentVariables,
 * currentDifferentiable and an empty resultInputBindings array. Variable arrays are copied on each restore.
 * Missing state returns null for the host's full-clear path. The host must cancel pending Function work
 * before calling, assign the returned globals before refreshing derivative controls, then schedule rendered
 * and solver fitting. The function invalidates old render ownership before projecting any saved DOM.
 */
void lab_workspace_dom_result_restore(int state);

#endif
