/**
 * @file lab_evaluation_install.h
 * @brief Private ordered evaluation-completion plans for the MARS Lab browser.
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

/** @brief Publish the ordered services for one accepted evaluation completion phase.
 * @param mode Worksheet mode index 0..6.
 * @param phase Initial projection 0 or completion 1; other values produce null.
 * @param outcome Accepted outcome 2 or partial Expression outcome 3.
 * @param data Scoped native response, retained as opaque browser values in service arguments.
 * @param context Scoped evaluation context containing authored text and captured editor/binding values.
 * @param cards Scoped card record previously returned by lab_evaluation_cards.
 * @param view Scoped live getter view containing editors, fullText and rawEditor;
 * custom views may supply expressionText instead of rawEditor.
 * @details Publishes calls through lab_dom_return. Invalid modes/outcomes produce null.
 * The caller must run phase zero services before requesting phase one, stop on service
 * failure, and check request ownership after every asynchronous boundary. Plan creation
 * does not change the DOM or redefine the host's live state properties.
 */
void lab_evaluation_install(unsigned mode, unsigned phase, unsigned outcome, int data, int context, int cards,
                            int view);

#endif
