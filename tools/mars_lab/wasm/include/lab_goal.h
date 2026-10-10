/**
 * @file lab_goal.h
 * @brief Goal-seek completion policy for the freestanding Lab browser.
 *
 * Produces ordered browser-owned service plans without interpreting mathematical
 * text or retaining handles. Native continuations own requests and freshness checks;
 * the JavaScript adapter supplies promises and live getters. Services execute only after the
 * synchronous handle scope ends. This is a private browser ABI, not a MARS API.
 */
#ifndef LAB_WASM_GOAL_H
#define LAB_WASM_GOAL_H

/**
 * @brief Publish an ordered goal-seek completion plan through lab_dom_return.
 * @param outcome Rejected response 1, accepted response 2 or 3, or caught exception 4.
 * @param data Scoped native response; accepted response fields are read by deferred services.
 * @param completion Scoped successful completion context containing options, cards, editorBody,
 * editorExpression, seek and target. Ignored on failure.
 * @param error Scoped exception text for outcome 4, including an empty string.
 * @details Returns calls and a boolean result; invalid or stale outcomes publish null.
 * History precedes editor replacement. No browser state is changed while producing the plan.
 */
void lab_goal_complete(unsigned outcome, int data, int completion, int error);

/**
 * @brief Publish the successful completion label through lab_dom_return.
 * @param cards Scoped native card projection whose unchanged property is read at execution time.
 * @details Returns Goal already reached when unchanged is truthy, otherwise Goal reached.
 */
void lab_goal_status(int cards);

#endif
