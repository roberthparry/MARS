/**
 * @file lab_result.h
 * @brief Application API: native result-card projection, presentation caches and browser copy policy.
 *
 * Installs server-authored text, HTML and SVG without interpreting mathematics.
 * This browser module selects representations, bounds presentation caches,
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

#include <stdbool.h>

/** @brief Text-card metadata selection. */
typedef enum {
    LAB_RESULT_TEXT_AUTO = 0, /**< Choose metadata from card identity. */
    LAB_RESULT_TEXT_FUNCTION = 1, /**< Force Function metadata. */
    LAB_RESULT_TEXT_HEADING = 2 /**< Force matrix-heading metadata. */
} lab_result_text_kind_t;

/** @brief Native result rendering families. */
typedef enum {
    LAB_RESULT_EXPRESSION = 0, /**< Expression rendering. */
    LAB_RESULT_MATRIX = 1, /**< Matrix rendering. */
    LAB_RESULT_CALCULUS = 2 /**< Derivative or integral rendering. */
} lab_result_kind_t;

/** @brief Temporary clipboard feedback. */
typedef enum {
    LAB_COPY_RESET = 0, /**< Restore the original button label. */
    LAB_COPY_SUCCESS = 1, /**< Report a successful copy. */
    LAB_COPY_FAILURE = 2 /**< Report a failed copy. */
} lab_copy_state_t;

/**
 * @brief Cache a native editor and its trimmed expression alias.
 * @param cache Scoped browser Map keyed by trimmed editor text and expression aliases.
 * @param editor Scoped native editor metadata with text and optional expression strings.
 * @details The browser Map retains metadata objects; no integer handle survives the call.
 */
void lab_result_editor_install(int cache, int editor);

/**
 * @brief Install native presentation metadata with bounded cache eviction.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param data Scoped native response with editor and presentation records, including expansion permissions,
 * editor aliases, numeric solution lines, compact text and solver TeX mappings.
 */
void lab_result_presentation_install(int caches, int data);

/**
 * @brief Cache exact-source Function and matrix markup.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param data Scoped native response whose presentation.function_syntax and presentation.matrix_headings
 * arrays contain source-keyed metadata records.
 */
void lab_result_syntax_install(int caches, int data);

/**
 * @brief Test native permission for an exact display/full-text pair.
 * @param cache Scoped expansions Map from display strings to Maps of full strings and Boolean permissions.
 * @param display Scoped compact-text string used as the outer cache key.
 * @param full Scoped full-text string used as the inner cache key.
 * @return True only when the matching native can_expand flag is Boolean true; false for missing or false entries.
 */
bool lab_result_can_expand(int cache, int display, int full);

/**
 * @brief Read native Function classification for an exact source.
 * @param cache Scoped Function syntax Map keyed by exact source strings.
 * @param source Scoped source string to look up without parsing or normalisation.
 * @return True only when the matching metadata's is_function field is Boolean true; false otherwise.
 */
bool lab_result_is_function(int cache, int source);

/**
 * @brief Install matching native markup, otherwise literal source text.
 * @param element Scoped destination DOM node.
 * @param source Scoped exact source string used for metadata matching and literal fallback.
 * @param metadata Scoped native syntax record with source and html fields, or zero when no metadata exists.
 */
void lab_result_native_text(int element, int source, int metadata);

/**
 * @brief Select native syntax metadata and render a text card.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param element Scoped destination DOM node; automatic selection recognises the Function card by node identity.
 * @param source Scoped exact source string used as the cache key and fallback text.
 * @param kind Automatic classification 0, force Function metadata 1 or force matrix-heading metadata 2.
 */
void lab_result_text_render(int caches, int element, int source, lab_result_text_kind_t kind);

/**
 * @brief Install compact/full text and reset its metadata-controlled expansion button.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param element Scoped destination text-card node; stores both text variants in its dataset.
 * @param button Scoped more-digits control associated with the destination.
 * @param display Scoped compact text string; an empty value falls back to full for the initial display.
 * @param full Scoped complete native text string retained for expansion and copying.
 * @return True if element is the Function card node, false otherwise; this is not an expansion-availability flag.
 */
bool lab_result_expandable(int caches, int element, int button, int display, int full);

/**
 * @brief Install numerical text and clear its explanatory note.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param full Scoped complete native numerical text string; used for both compact and full variants.
 */
void lab_result_value(int caches, int full);

/**
 * @brief Validate all DateTime sections before replacing the destination card.
 * @param element Scoped destination card node; the Rendered card also invalidates pending digit rendering.
 * @param button Scoped more-digits control to disable after projection.
 * @param sections Scoped array of native section records with rows and html fields; non-arrays mean no sections.
 * @param fallback Scoped plain-text fallback/copy source, trimmed before storage.
 * @return True after projection, false without mutation if a section with rows lacks nonblank native HTML.
 */
bool lab_result_datetime(int element, int button, int sections, int fallback);

/**
 * @brief Project native matrix expression representations.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param data Scoped native matrix response with expression_pretty, display_expression_pretty, expression
 * and result text; the most specific available representation is selected.
 */
void lab_result_matrix_expression(int caches, int data);

/**
 * @brief Project native matrix values and optional SVG.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param data Scoped native matrix response with value text and optional value_svg markup.
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

/**
 * @brief Project native rendering metadata and publish its TeX copy source.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param data Scoped native response with TeX, SVG, display/full variants and render-error metadata.
 * @param kind Expression 0, Matrix 1 or Calculus 2; other values publish null without rendering.
 * @details The copy source is returned through lab_dom_return, not as a C return value.
 */
void lab_result_rendered(int caches, int data, lab_result_kind_t kind);

/**
 * @brief Project Matrix or Calculus result cards and publish their transfer metadata.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param data Scoped native response with expression/result, value, Function and rendering representations;
 * matrix responses may also carry binding_values.
 * @param kind Matrix 1 or Calculus 2; other values publish null without projecting cards.
 * @details lab_dom_return supplies a record containing TeX and copied bindings.
 */
void lab_result_display(int caches, int data, lab_result_kind_t kind);

/**
 * @brief Toggle a card's exact native text representations and accessible button state.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param element Scoped text-card node with displayText and fullText dataset entries.
 * @param button Scoped more-digits control whose expanded dataset flag selects the next variant.
 */
void lab_result_text_digits(int caches, int element, int button);

/**
 * @brief Apply a cached digit toggle or publish an owned full-digit render request.
 * @param last_TeX Scoped most recent TeX copy-source string, used when the card lacks fullTex.
 * @details lab_dom_return supplies a request object with TeX and retained browser identity, or null when
 * no fetch is needed. The pending request disables the more-digits button until completed or invalidated.
 */
void lab_result_digits_begin(int last_TeX);

/**
 * @brief Complete a matching full-digit request without overwriting a newer card.
 * @param request Scoped browser request object returned by lab_result_digits_begin; identity must still match.
 * @param data Scoped native render response with svg and render_error fields, or zero on transport failure.
 * @param error Scoped transport-error text, or a false-like value to use data.render_error.
 * @return True when the owned completion was projected; false for an absent or obsolete request.
 */
bool lab_result_digits_finish(int request, int data, int error);

/**
 * @brief Publish the selected target's copy representation through lab_dom_return.
 * @param target Scoped target-name string: rendered, plot, expression, function, value or mobile.
 * @param last_TeX Scoped latest TeX copy-source string, used for a non-error Rendered card.
 * @details Unknown targets and mobile URLs without an HTTP(S) scheme publish an empty string.
 */
void lab_result_copy(int target, int last_TeX);

/**
 * @brief Project reusable expression text and button availability.
 * @param text Scoped native result text, trimmed for storage; blank text hides and disables Use as input.
 */
void lab_result_input_set(int text);

/**
 * @brief Publish reusable expression text for the supplied mode.
 * @param mode Scoped worksheet mode-name string; equation uses the displayed Expression card, while other
 * modes prefer the explicit reusable source when present.
 * @details Publishes trimmed opaque text through lab_dom_return.
 */
void lab_result_input_get(int mode);

/**
 * @brief Project temporary copy feedback while retaining the original button label.
 * @param button Scoped copy-control node whose original label is retained in its dataset.
 * @param state Reset original label 0, success 1 or failure 2; the browser owns the reset timer.
 */
void lab_result_copy_flash(int button, lab_copy_state_t state);

/**
 * @brief Project matrix fallback text and publish an owned presentation request.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param element Scoped matrix text-card node receiving fallback markup and browser request identity.
 * @param button Scoped more-digits control to disable for this matrix presentation.
 * @param result Scoped native result string used when pretty is absent or empty.
 * @param pretty Scoped preferred native pretty-text string, or a false-like value to use result.
 * @details lab_dom_return supplies a request record with text; the same browser object identifies completion.
 */
void lab_result_pretty_begin(int caches, int element, int button, int result, int pretty);

/**
 * @brief Install matrix markup only while the request and displayed source still match.
 * @param element Scoped matrix card node retaining the pending presentation request.
 * @param request Scoped request object returned by lab_result_pretty_begin; identity and text must still match.
 * @param data Scoped native response whose non-empty html string is installed for a current request.
 */
void lab_result_pretty_finish(int element, int request, int data);

/** @brief Clear rendered content, expansion and pending digit rendering.
 */
void lab_result_pane_clear(void);

/**
 * @brief Clear result metadata, plot content and result controls.
 * @param caches Scoped browser presentation-cache record, used when resetting the numerical Value card.
 * @param mode Scoped worksheet mode-name string; expression mode additionally hides the empty Value card.
 */
void lab_result_details_clear(int caches, int mode);

/**
 * @brief Clear result cards and publish reset metadata with optional binding-clear effects.
 * @param caches Scoped browser record of presentation cache Maps (editors, expansions, functions, headings,
 * numeric, compact and solver); only the maps required by this operation are consulted.
 * @param mode Scoped active worksheet mode-name string.
 * @param options Scoped options record; a truthy keepBindings retains authored binding controls.
 * @details lab_dom_return supplies cleared result metadata and a calls array, including clearVariableValues
 * unless keepBindings was requested.
 */
void lab_result_reset(int caches, int mode, int options);

/**
 * @brief Copy native result bindings for a later editor transfer.
 * @param bindings Scoped browser array of native binding records; other types produce an empty array.
 * @details lab_dom_return supplies a new array, copying each present record's own string-keyed metadata
 * and preserving sparse array holes. Copied browser values, not scoped handle integers, survive the call.
 */
void lab_result_binding_snapshot(int bindings);

/**
 * @brief Validate and publish a native Almanac presentation variant.
 * @param data Scoped native response whose almanac_presentation record contains named variants.
 * @param visibility Scoped visibility-key string used to select a variant from almanac_presentation.
 * @return True when the selected variant has nonblank HTML, false otherwise.
 * @details lab_dom_return separately publishes the validated variant object, or null when validation fails.
 */
bool lab_result_almanac_variant(int data, int visibility);

/**
 * @brief Project validated Almanac markup and its copy source.
 * @param target Scoped destination card node; the Rendered card also invalidates pending digit rendering.
 * @param variant Scoped previously validated presentation record with html and copy_text strings.
 */
void lab_result_almanac_render(int target, int variant);

#endif
