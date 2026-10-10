/**
 * @file lab_result.h
 * @brief Private interface: native result-card projection, presentation caches and browser copy policy.
 *
 * Installs server-authored text, HTML and SVG without interpreting mathematics.
 * This private browser module selects representations, bounds presentation caches,
 * controls digit expansion and rejects obsolete asynchronous render completions.
 * Browser imports provide generic DOM, object and Map operations. All handles are
 * borrowed within one synchronous call; persistent objects belong to the host.
 * Calendar markup is validated before changing the visible result, allowing the
 * JavaScript request boundary to preserve its diagnostic and recovery behaviour.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_RESULT_H
#define LAB_WASM_RESULT_H

/** @brief Cache a native editor and its trimmed expression alias.
 * @param cache Scoped cache value; never retained as a handle.
 * @param editor Scoped editor value; never retained as a handle.
 */
void lab_result_editor_install(int cache, int editor);

/** @brief Install native presentation metadata with bounded cache eviction.
 * @param caches Scoped caches value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 */
void lab_result_presentation_install(int caches, int data);

/** @brief Cache exact-source Function and matrix markup.
 * @param caches Scoped caches value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 */
void lab_result_syntax_install(int caches, int data);

/** @brief Test native permission for the exact display/full pair.
 * @param cache Scoped cache value; never retained as a handle.
 * @param display Scoped display value; never retained as a handle.
 * @param full Scoped full value; never retained as a handle.
 * @return Native success/classification flag; returned values use lab_dom_return where documented.
 */
int lab_result_can_expand(int cache, int display, int full);

/** @brief Return native Function classification for an exact source.
 * @param cache Scoped cache value; never retained as a handle.
 * @param source Scoped source value; never retained as a handle.
 * @return Native success/classification flag; returned values use lab_dom_return where documented.
 */
int lab_result_is_function(int cache, int source);

/** @brief Install matching native markup, otherwise literal source text.
 * @param element Scoped element value; never retained as a handle.
 * @param source Scoped source value; never retained as a handle.
 * @param metadata Scoped metadata value; never retained as a handle.
 */
void lab_result_native_text(int element, int source, int metadata);

/** @brief Render automatic (0), Function (1) or matrix (2) metadata.
 * @param caches Scoped caches value; never retained as a handle.
 * @param element Scoped element value; never retained as a handle.
 * @param source Scoped source value; never retained as a handle.
 * @param kind Representation selector described above.
 */
void lab_result_text_render(int caches, int element, int source, unsigned kind);

/** @brief Install expandable text; return whether the Function card changed.
 * @param caches Scoped caches value; never retained as a handle.
 * @param element Scoped element value; never retained as a handle.
 * @param button Scoped button value; never retained as a handle.
 * @param display Scoped display value; never retained as a handle.
 * @param full Scoped full value; never retained as a handle.
 * @return Native success/classification flag; returned values use lab_dom_return where documented.
 */
int lab_result_expandable(int caches, int element, int button, int display, int full);

/** @brief Install numerical text and clear its explanatory note.
 * @param caches Scoped caches value; never retained as a handle.
 * @param full Scoped full value; never retained as a handle.
 */
void lab_result_value(int caches, int full);

/** @brief Validate and project DateTime sections; return zero without mutation on missing markup.
 * @param element Scoped element value; never retained as a handle.
 * @param button Scoped button value; never retained as a handle.
 * @param sections Scoped sections value; never retained as a handle.
 * @param fallback Scoped fallback value; never retained as a handle.
 * @return Native success/classification flag; returned values use lab_dom_return where documented.
 */
int lab_result_datetime(int element, int button, int sections, int fallback);

/** @brief Project native matrix expression representations.
 * @param caches Scoped caches value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 */
void lab_result_matrix_expression(int caches, int data);

/** @brief Project native matrix values and optional SVG.
 * @param caches Scoped caches value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 */
void lab_result_matrix_value(int caches, int data);

/** @brief Invalidate pending full-digit rendering and release its button.
 */
void lab_result_render_invalidate(void);

/** @brief Discard the current restored solver-card identity, invalidating its late replies. */
void lab_result_solver_invalidate(void);

/** @brief Give a restored differential-equation solver card a fresh, scoped-mode browser identity.
 * Removes its expired evaluation-parent token. Does nothing when no eligible card is displayed.
 * The identity retains actual browser values, not temporary bridge handles.
 */
void lab_result_solver_restore(void);

/** @brief Publish the restored solver-card identity through lab_dom_return.
 * Returns the browser-owned identity only in its original mode generation with an eligible card;
 * otherwise publishes null. Callers compare identity again after awaiting rendering.
 */
void lab_result_solver_owner(void);

/** @brief Project Expression (0), Matrix (1) or Calculus (2) rendering; return the copy source.
 * @param caches Scoped caches value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 * @param kind Representation selector described above.
 */
void lab_result_rendered(int caches, int data, unsigned kind);

/** @brief Project Matrix (1) or Calculus (2) cards; return TeX and copied bindings.
 * @param caches Scoped caches value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 * @param kind Representation selector described above.
 */
void lab_result_display(int caches, int data, unsigned kind);

/** @brief Toggle exact native text representations and button state.
 * @param caches Scoped caches value; never retained as a handle.
 * @param element Scoped element value; never retained as a handle.
 * @param button Scoped button value; never retained as a handle.
 */
void lab_result_text_digits(int caches, int element, int button);

/** @brief Apply a cached digit toggle or return a host-owned render request.
 * @param last_TeX Scoped last TeX value; never retained as a handle.
 */
void lab_result_digits_begin(int last_TeX);

/** @brief Complete a matching digit request; return zero for obsolete requests.
 * @param request Scoped request value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 * @param error Scoped error value; never retained as a handle.
 * @return Native success/classification flag; returned values use lab_dom_return where documented.
 */
int lab_result_digits_finish(int request, int data, int error);

/** @brief Return the selected target's copy text through the host.
 * @param target Scoped target value; never retained as a handle.
 * @param last_TeX Scoped last TeX value; never retained as a handle.
 */
void lab_result_copy(int target, int last_TeX);

/** @brief Project reusable expression text and button availability.
 * @param text Scoped text value; never retained as a handle.
 */
void lab_result_input_set(int text);

/** @brief Return reusable expression text for the supplied mode.
 * @param mode Scoped mode value; never retained as a handle.
 */
void lab_result_input_get(int mode);

/** @brief Project copy feedback: reset (0), success (1) or failure (2).
 * @param button Scoped button value; never retained as a handle.
 * @param state Feedback selector described above.
 */
void lab_result_copy_flash(int button, unsigned state);

/** @brief Project matrix fallback text and return a host-owned presentation request.
 * @param caches Scoped caches value; never retained as a handle.
 * @param element Scoped element value; never retained as a handle.
 * @param button Scoped button value; never retained as a handle.
 * @param result Scoped result value; never retained as a handle.
 * @param pretty Scoped pretty value; never retained as a handle.
 */
void lab_result_pretty_begin(int caches, int element, int button, int result, int pretty);

/** @brief Install matrix markup only when request identity and displayed source still match.
 * @param element Scoped element value; never retained as a handle.
 * @param request Scoped request value; never retained as a handle.
 * @param data Scoped data value; never retained as a handle.
 */
void lab_result_pretty_finish(int element, int request, int data);

/** @brief Clear rendered content, expansion and pending digit rendering.
 */
void lab_result_pane_clear(void);

/** @brief Clear result metadata and controls for the supplied mode.
 * @param caches Scoped caches value; never retained as a handle.
 * @param mode Scoped mode value; never retained as a handle.
 */
void lab_result_details_clear(int caches, int mode);

/** @brief Clear cards and return native reset values with optional binding-clear effects.
 * @param caches Borrowed native-presentation cache maps.
 * @param mode Borrowed active worksheet label.
 * @param options Borrowed options; a truthy keepBindings retains authored binding controls.
 */
void lab_result_reset(int caches, int mode, int options);

/** @brief Copy native result bindings for a later editor transfer.
 * @param bindings Borrowed array; other values produce an empty array.
 * @details Copies each present record's own string-keyed metadata and preserves sparse array holes.
 */
void lab_result_binding_snapshot(int bindings);

/** @brief Return a validated native Almanac variant or null through the host.
 * @param data Scoped data value; never retained as a handle.
 * @param visibility Scoped visibility value; never retained as a handle.
 * @return Native success/classification flag; returned values use lab_dom_return where documented.
 */
int lab_result_almanac_variant(int data, int visibility);

/** @brief Project validated Almanac markup and its copy source.
 * @param target Scoped target value; never retained as a handle.
 * @param variant Scoped variant value; never retained as a handle.
 */
void lab_result_almanac_render(int target, int variant);

#endif
