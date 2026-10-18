/**
 * @file lab_evaluation_setup.h
 * @brief Application API for staged worksheet request preparation.
 *
 * C selects binding capture, commits, calendar snapshots and authored-input
 * precedence. The browser executes each ordered plan outside the scoped bridge,
 * awaits only its final service when requested and checks request ownership before
 * continuing. Mathematical text is opaque; native workers still perform parsing.
 * No handles are retained between stages. This is a browser ABI, not a library API.
 */
#ifndef LAB_WASM_EVALUATION_SETUP_H
#define LAB_WASM_EVALUATION_SETUP_H

#include "lab_handle.h"
#include "lab_mode.h"

/** @brief Preparation stage on either side of binding commit and expression assembly. */
typedef enum {
    LAB_EVALUATION_SETUP_BINDINGS = 0, /**< Capture visible bindings and plan any required commit. */
    LAB_EVALUATION_SETUP_INPUT = 1,    /**< Capture source or calendar input and assemble Expression input. */
    LAB_EVALUATION_SETUP_ASSEMBLED = 2 /**< Select the assembled Expression source and save its editor state. */
} lab_evaluation_setup_phase_t;

/**
 * @brief Prepare one synchronous stage and publish its deferred browser services.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param phase LAB_EVALUATION_SETUP_BINDINGS (0), LAB_EVALUATION_SETUP_INPUT (1),
 * or LAB_EVALUATION_SETUP_ASSEMBLED (2); the final stage is valid only for Expression.
 * @param context Scoped writable request-local browser record; receives text, editorText and editorBodyText.
 * The assembly service supplies entered text before phase 2.
 * @param options Scoped caller options; reuseLastInput is inspected only after expression assembly.
 * @param view Scoped read-only view with lazy rawEditor/editors, bodyText and lastInput properties;
 * custom views may supply expressionText instead of rawEditor.
 * @details Publishes calls, wait and done through lab_dom_return. Only the last service
 * may be asynchronous; wait requests that it be awaited before the next phase.
 * Invalid modes/phases and phase 2 outside Expression publish null. The context must
 * not be shared with another request. Browser getters must not open a nested DOM scope.
 */
void lab_evaluation_setup(lab_mode_t mode, lab_evaluation_setup_phase_t phase, browser_jsref context,
                          browser_jsref options, view_jsref view);

#endif
