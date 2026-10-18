/**
 * @file lab_goal.h
 * @brief Goal-seek completion policy for the freestanding Lab browser.
 *
 * Produces ordered browser-owned service plans without interpreting mathematical
 * text or retaining handles. Native continuations own requests and freshness checks;
 * the JavaScript adapter supplies promises and live getters. Services execute only after the
 * synchronous handle scope ends. This is an application ABI, not an installed MARS library API.
 */
#ifndef LAB_WASM_GOAL_H
#define LAB_WASM_GOAL_H

#include "lab_requests.h"

/**
 * @brief Publish an ordered goal-seek completion plan through lab_dom_return.
 * @param outcome Stale 0, rejected response 1, accepted response 2 or 3, or caught exception 4.
 * @param data Scoped browser handle to the native response; failure reads error, while accepted response
 * fields are read by deferred services. Ignored for exceptions.
 * @param completion Scoped browser handle to the successful completion context containing options, cards, editorBody,
 * editorExpression, seek and target. Ignored on failure.
 * @param error Scoped browser handle to exception text for outcome 4, including an empty string; ignored otherwise.
 * @post Publishes calls and a boolean result through lab_dom_return; invalid or stale outcomes publish null.
 * History precedes editor replacement. No browser state is changed while producing the plan.
 */
void lab_goal_complete(lab_request_outcome_t outcome, int data, int completion, int error);

/**
 * @brief Publish the successful completion label through lab_dom_return.
 * @param cards Scoped browser handle to the native card projection whose unchanged property is read at execution time.
 * @post Publishes Goal already reached through lab_dom_return when unchanged is truthy, otherwise Goal reached.
 */
void lab_goal_status(int cards);

#endif
