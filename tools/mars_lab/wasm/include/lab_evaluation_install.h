/**
 * @file lab_evaluation_install.h
 * @brief Application API for ordered evaluation-completion plans for the MARS Lab browser.
 *
 * Selects editor, result-card, binding and persistence effects for all seven modes.
 * The host executes the returned services outside the scoped DOM bridge. Two phases
 * preserve live editor reads after projection; differential equations resume phase
 * one only after their optional SVG request and a fresh ownership check. Native
 * mathematical text stays opaque. This is a tool-private main-thread interface;
 * no scoped handles are retained between calls.
 */
#ifndef LAB_WASM_EVALUATION_INSTALL_H
#define LAB_WASM_EVALUATION_INSTALL_H

#include "lab_mode.h"
#include "lab_requests.h"

/** @brief Installation phase separated by editor projection and optional solver rendering. */
typedef enum {
    LAB_EVALUATION_INSTALL_PROJECT = 0, /**< Install initial editor and card presentation. */
    LAB_EVALUATION_INSTALL_COMPLETE = 1 /**< Complete cards and persistence after projection. */
} lab_evaluation_install_phase_t;

/**
 * @brief Publish the ordered services for one accepted evaluation completion phase.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param phase LAB_EVALUATION_INSTALL_PROJECT (0) or LAB_EVALUATION_INSTALL_COMPLETE (1);
 * other values produce null.
 * @param outcome Accepted success 2, or partial Expression result 3; mode 0 alone permits partial results.
 * @param data Scoped native response, retained as opaque browser values in service arguments.
 * @param context Scoped evaluation context with text, editorText, editorBodyText and enteredBindings captured
 * during request preparation; each mode reads only the fields relevant to its projection.
 * @param cards Scoped card record previously returned by lab_evaluation_cards.
 * @param view Scoped live getter view containing editors, fullText and rawEditor;
 * custom views may supply expressionText instead of rawEditor.
 * @details Publishes calls through lab_dom_return. Invalid modes/outcomes produce null.
 * The caller must run phase zero services before requesting phase one, stop on service
 * failure, and check request ownership after every asynchronous boundary. Plan creation
 * does not change the DOM or redefine the host's live state properties.
 */
void lab_evaluation_install(lab_mode_t mode, lab_evaluation_install_phase_t phase, lab_request_outcome_t outcome,
                            int data, int context, int cards, int view);

#endif
