/**
 * @file lab_evaluation_cards.h
 * @brief Private browser interface for evaluation card selection and presentation.
 *
 * Selects server-authored representations for the seven worksheet modes and goal
 * results, composes solver diagnostics, merges weather metadata and prepares goal
 * actions. Native continuations select request and cancellation checks, while the
 * browser supplies promises and the result and workspace modules project the selected
 * content. This interface never parses or rewrites mathematics.
 *
 * All integer object arguments are scoped browser handles unless documented as
 * mode indices. Calls run synchronously on the main browser thread and retain no
 * handles. Returned JavaScript records are delivered through lab_dom_return; the
 * host owns those records after its synchronous handle scope closes. This is a
 * tool-private interface, not an installed MARS library API.
 */
#ifndef LAB_WASM_EVALUATION_CARDS_H
#define LAB_WASM_EVALUATION_CARDS_H

/**
 * @brief Select exact native representations and mode-specific card metadata.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3,
 * integrator 4, DateTime 5, almanac 6 or goal result 7.
 * @param data Scoped native evaluation response.
 * @param text Scoped authored source string used for native fallback selection.
 * @param caches Scoped result-cache object containing editor, expansion and solver Maps.
 * @return No C value. The host receives a card-plan object, or null for an invalid mode.
 * @details Common fields are expression, full_expression, input, function,
 * full_function and value. Expression/goal plans add derivative, differentiable
 * and unchanged; equation adds expandable; differential equation adds solver_source;
 * matrix adds scalar; DateTime adds calendar records containing element, button,
 * sections and text. All source strings remain opaque native representations.
 */
void lab_evaluation_cards(unsigned mode, int data, int text, int caches);

/**
 * @brief Project mode-specific notes, controls and visibility after installing card text.
 * @param mode Worksheet mode index 0..6; unsupported indices perform no action.
 * @param data Scoped native evaluation response.
 * @return No value. Expression updates notes and numerical visibility; differential
 * equation clears the old solver class; matrix updates its title and operation;
 * calendar modes reset the rendered digit button.
 * @details Run after the relevant text services so expression visibility reflects
 * the installed numerical content. Solver markup is subsequently installed only
 * after the asynchronous owner checks request freshness.
 */
void lab_evaluation_cards_present(unsigned mode, int data);

/**
 * @brief Select date and location fields for a dependent weather request.
 * @param state Scoped captured DateTime state with date, latitude and longitude fields.
 * @param data Scoped native response whose fields object may override those values.
 * @return No C value. The host receives a shallow state copy with the three selected
 * fields converted to strings; the supplied records are not mutated.
 * @details Truthy evaluation fields take precedence over the captured state, matching
 * the existing request contract. Unrelated state fields retain their values.
 */
void lab_evaluation_weather_state(int state, int data);

/**
 * @brief Merge native weather sections with the evaluated DateTime overview.
 * @param overview Scoped original evaluation response.
 * @param weather Scoped successful weather response.
 * @return No C value. The host receives sections and text for the calendar renderer.
 * @details Existing sections whose title is exactly Weather are removed, then all
 * supplied weather sections are appended. Non-array section collections are ignored.
 * Non-empty overview strings are joined by a newline. HTML validation remains the
 * calendar renderer's responsibility; missing markup is never fabricated here.
 */
void lab_evaluation_weather_cards(int overview, int weather);

/**
 * @brief Prepare a goal-seek click or reveal its target entry.
 * @param mode Current worksheet mode index; only Expression 0 is eligible.
 * @param text Scoped current expression source string, which must be non-empty.
 * @return No C value. The host receives null when ineligible or after revealing the
 * target entry, otherwise a record containing text and the trimmed target string.
 * @details An empty target becomes 0. A ready action reveals the result pane; the
 * browser then starts the asynchronous request and commits bindings. Focus and
 * selection effects occur after the scoped native call has finished.
 */
void lab_evaluation_goal_start(unsigned mode, int text);

#endif
