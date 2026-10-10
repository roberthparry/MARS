/**
 * @file lab_flow_result.h
 * @brief Native continuations for result actions and worksheet startup.
 *
 * Shared browser dispatch uses this private interface to sequence editor transfers,
 * native layout requests, clipboard publication and initial state restoration.
 * Browser frames own values and promises; scoped handles never survive a call.
 */
#ifndef LAB_FLOW_RESULT_H
#define LAB_FLOW_RESULT_H

/** @brief Advance a result-action continuation.
 * @param kind Transfer 0, pretty matrix 1, expanded TeX 2, input copy 3, card copy 4,
 * TeX request 5, initial worksheet 6.
 * @param frame Borrowed mutable browser-owned frame.
 * @param view Borrowed read-only presentation view.
 * @return Scoped plan for the generic asynchronous browser interpreter.
 */
int lab_flow_result(unsigned kind, int frame, int view);

#endif
