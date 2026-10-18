/**
 * @file lab_evaluation_cards.h
 * @brief Application interface for evaluation card selection and presentation.
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
 * application interface, not an installed MARS library API.
 */
#ifndef LAB_WASM_EVALUATION_CARDS_H
#define LAB_WASM_EVALUATION_CARDS_H

#include "lab_handle.h"
#include "lab_mode.h"

/** @brief Result-card presentation kind; worksheet values share the lab_mode_t ABI. */
typedef enum {
    LAB_EVALUATION_CARDS_EXPRESSION = LAB_MODE_EXPRESSION, /**< Ordinary Expression result. */
    LAB_EVALUATION_CARDS_EQUATION = LAB_MODE_EQUATION, /**< Equation result. */
    LAB_EVALUATION_CARDS_DIFFERENTIAL_EQUATION = LAB_MODE_DIFFERENTIAL_EQUATION, /**< Differential-equation result. */
    LAB_EVALUATION_CARDS_MATRIX = LAB_MODE_MATRIX, /**< Matrix result. */
    LAB_EVALUATION_CARDS_INTEGRATOR = LAB_MODE_INTEGRATOR, /**< Integration result. */
    LAB_EVALUATION_CARDS_DATETIME = LAB_MODE_DATETIME, /**< DateTime result. */
    LAB_EVALUATION_CARDS_ALMANAC = LAB_MODE_ALMANAC, /**< Almanac result with its own worksheet renderer. */
    LAB_EVALUATION_CARDS_GOAL = 7 /**< Goal-seek result, not a selectable worksheet mode. */
} lab_evaluation_card_kind_t;

/**
 * @brief Select exact native representations and mode-specific card metadata.
 * @param kind Card presentation kind; worksheet values 0..6 match lab_mode_t,
 * while LAB_EVALUATION_CARDS_GOAL (7) selects goal-result presentation.
 * @param data Scoped native evaluation response.
 * @param text Scoped authored source string used for native fallback selection.
 * @param caches Scoped result-cache object with editors, expansions and solver Maps, keyed by exact native text.
 * @post Publishes a card-plan object through lab_dom_return, or null for an invalid mode.
 * @details Common fields are expression, full_expression, input, function,
 * full_function and value. Expression/goal plans add derivative, differentiable
 * and unchanged; equation adds expandable; differential equation adds solver_source;
 * matrix adds scalar; DateTime adds calendar records containing element, button,
 * sections and text. All source strings remain opaque native representations.
 */
void lab_evaluation_cards(lab_evaluation_card_kind_t kind, browser_jsref data, browser_jsref text, browser_jsref caches);

/**
 * @brief Project mode-specific notes, controls and visibility after installing card text.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * Modes 1 and 4, and unsupported indices, perform no additional projection.
 * @param data Scoped native evaluation response.
 * @post Expression updates notes and numerical visibility; differential
 * equation clears the old solver class; matrix updates its title and operation;
 * calendar modes reset the rendered digit button.
 * @details Run after the relevant text services so expression visibility reflects
 * the installed numerical content. Solver markup is subsequently installed only
 * after the asynchronous owner checks request freshness.
 */
void lab_evaluation_cards_present(lab_mode_t mode, browser_jsref data);

/**
 * @brief Select date and location fields for a dependent weather request.
 * @param state Scoped captured DateTime state with date, latitude and longitude fields.
 * @param data Scoped native response whose fields object may override date, latitude and longitude;
 * coordinate strings retain the native request units without conversion.
 * @post Publishes a shallow state copy through lab_dom_return with the three selected
 * fields converted to strings; the supplied records are not mutated.
 * @details Truthy evaluation fields take precedence over the captured state, matching
 * the existing request contract. Unrelated state fields retain their values.
 */
void lab_evaluation_weather_state(browser_jsref state, browser_jsref data);

/**
 * @brief Merge native weather sections with the evaluated DateTime overview.
 * @param overview Scoped original response containing overview_sections and overview copy text.
 * @param weather Scoped weather response whose overview_sections and overview text replace old Weather content.
 * @post Publishes sections and text for the calendar renderer through lab_dom_return.
 * @details Existing sections whose title is exactly Weather are removed, then all
 * supplied weather sections are appended. Non-array section collections are ignored.
 * Non-empty overview strings are joined by a newline. HTML validation remains the
 * calendar renderer's responsibility; missing markup is never fabricated here.
 */
void lab_evaluation_weather_cards(browser_jsref overview, browser_jsref weather);

/**
 * @brief Prepare a goal-seek click or reveal its target entry.
 * @param mode Current worksheet mode index; only Expression 0 is eligible.
 * @param text Scoped current expression source string, which must be non-empty.
 * @post Publishes null through lab_dom_return when ineligible or after revealing the
 * target entry, otherwise a record containing text and the trimmed target string.
 * @details An empty target becomes 0. A ready action reveals the result pane; the
 * browser then starts the asynchronous request and commits bindings. Focus and
 * selection effects occur after the scoped native call has finished.
 */
void lab_evaluation_goal_start(lab_mode_t mode, browser_jsref text);

#endif
