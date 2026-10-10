/**
 * @file lab_function.h
 * @brief Private Function-card execution presentation for the Lab browser.
 *
 * Selects the full native programme source and projects availability, progress and
 * completion into the existing Function card. Source and output remain opaque;
 * this module does not parse or execute programmes. The host owns cancellation,
 * asynchronous transport, action-running indicators and final button re-enabling.
 * All calls are synchronous and no scoped browser handle survives a call.
 */
#ifndef LAB_WASM_FUNCTION_H
#define LAB_WASM_FUNCTION_H

/**
 * @brief Publish the executable card source through lab_dom_return.
 * @details Publishes null when RUN is disabled or the title is not exactly Function.
 * Otherwise publishes the trimmed full-source dataset string, including an empty string.
 */
void lab_function_source(void);

/**
 * @brief Enable RUN, empty its output and hide the execution result.
 * @details The host must cancel the previous request first and separately clear its action-running indicator.
 */
void lab_function_clear(void);

/**
 * @brief Show programme availability or execution progress.
 * @param available Non-zero when the host obtained a request; disables RUN and displays Running….
 * Zero displays the no-programme diagnostic without changing RUN's disabled state.
 */
void lab_function_start(int available);

/**
 * @brief Project a current programme response or exception without altering request ownership.
 * @param outcome Stale 0, failure 1, success 2 or exception 4; other values are ignored.
 * @param timed_out Non-zero selects the timeout diagnostic on failure or exception, not success.
 * @param data Scoped response whose output and error fields are read in that order for outcomes 1 and 2.
 * @param error Scoped browser-owned message-text view for outcome 4, not a raw Error object.
 * Its message getter must return String(originalError.message), preserving undefined and null as text.
 * @details Trims only trailing output whitespace. Response diagnostics are fully trimmed.
 * Failure reads both response fields even when timed out; a timed-out exception never reads its message.
 * Only output text changes: the host retains final button re-enabling and request cleanup.
 */
void lab_function_complete(unsigned outcome, int timed_out, int data, int error);

#endif
