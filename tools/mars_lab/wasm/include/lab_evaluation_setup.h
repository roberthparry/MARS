/**
 * @file lab_evaluation_setup.h
 * @brief Private interface for staged worksheet request preparation.
 *
 * C selects binding capture, commits, calendar snapshots and authored-input
 * precedence. The browser executes each ordered plan outside the scoped bridge,
 * awaits only its final service when requested and checks request ownership before
 * continuing. Mathematical text is opaque; native workers still perform parsing.
 * No handles are retained between stages. This is a browser ABI, not a library API.
 */
#ifndef LAB_WASM_EVALUATION_SETUP_H
#define LAB_WASM_EVALUATION_SETUP_H

/**
 * @brief Prepare one synchronous stage and publish its deferred browser services.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param phase Binding preparation 0, input capture 1 or expression assembly completion 2.
 * @param context Scoped writable request-local record; text and editor snapshots are set when needed.
 * @param options Scoped caller options; reuseLastInput is inspected only after expression assembly.
 * @param view Scoped read-only view with lazy rawEditor/editors, bodyText and lastInput properties;
 * custom views may supply expressionText instead of rawEditor.
 * @details Publishes calls, wait and done through lab_dom_return. Only the last service
 * may be asynchronous; wait requests that it be awaited before the next phase.
 * Invalid modes/phases and phase 2 outside Expression publish null. The context must
 * not be shared with another request. Browser getters must not open a nested DOM scope.
 */
void lab_evaluation_setup(unsigned mode, unsigned phase, int context, int options, int view);

#endif
